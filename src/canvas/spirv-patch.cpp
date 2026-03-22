#include "canvas/spirv-patch.hpp"

namespace
{
// Layout
constexpr uint32_t k_header_words = 5;
constexpr uint32_t k_magic = 0x07230203;

// Opcodes
constexpr uint32_t k_op_nop = 0;
constexpr uint32_t k_op_ext_inst_import = 11;
constexpr uint32_t k_op_capability = 17;
constexpr uint32_t k_op_extension = 10;
constexpr uint32_t k_op_decorate = 71;
constexpr uint32_t OpLoad = 61;
constexpr uint32_t OpISub = 130;
constexpr uint32_t OpVariable = 59;
constexpr uint32_t OpCopyObject = 83;

// Decoration
constexpr uint32_t k_decoration_built_in = 11;

// BuiltIn values
constexpr uint32_t k_built_in_vertex_id = 5;
constexpr uint32_t k_built_in_instance_id = 6;
constexpr uint32_t BuiltInVertexIndex = 42;
constexpr uint32_t BuiltInInstanceIndex = 43;
constexpr uint32_t BuiltInBaseVertex = 4424;
constexpr uint32_t BuiltInBaseInstance = 4425;

// Capabilities
constexpr uint32_t k_capability_draw_parameters = 4427;

uint32_t GetWordCount(uint32_t word)
{
    return word >> 16;
}
uint32_t GetOpCode(uint32_t word)
{
    return word & 0xFFFF;
}

uint32_t CreateOp(uint32_t wc, uint32_t opcode)
{
    return (wc << 16) | (opcode & 0xFFFF);
}

// Nop-out an instruction: fill its words with single-word OpNop instructions.
void ReplaceWithNop(uint32_t* ptr, uint32_t word_count)
{
    for (uint32_t i = 0; i < word_count; ++i)
    {
        ptr[i] = CreateOp(1, k_op_nop);
    }
}

// Compare a SPIR-V literal string operand (packed into uint32s) against a C string.
bool CompareWithString(const uint32_t* words, uint32_t max_words, const char* str)
{
    const char* packed = reinterpret_cast<const char*>(words);
    const size_t max_len = static_cast<size_t>(max_words) * 4;
    for (size_t i = 0; i < max_len; ++i)
    {
        if (packed[i] != str[i])
        {
            return false;
        }
        if (str[i] == '\0')
        {
            return true;
        }
    }
    return false;
}
}  // namespace

void Rndr::Impl::PatchSpirv(Opal::DynamicArray<u32>& bytecode)
{
    if (bytecode.GetSize() < k_header_words)
    {
        throw Opal::Exception("Spir-V module too small, can't fit the header!");
    }
    if (bytecode[0] != k_magic)
    {
        throw Opal::Exception("Not a Spir-V module, bad magic value!");
    }

    // ── Pass 1: Scan decorations ────────────────────────────────────────────
    // Find variable IDs decorated with the builtins we care about.

    uint32_t id_instance_index = 0;
    uint32_t id_base_instance = 0;
    uint32_t id_vertex_index = 0;
    uint32_t id_base_vertex = 0;

    for (uint32_t i = k_header_words; i < bytecode.GetSize();)
    {
        const uint32_t wc = GetWordCount(bytecode[i]);
        const uint32_t op = GetOpCode(bytecode[i]);
        if (wc == 0)
        {
            throw Opal::Exception("Zero word count instruction detected!");
        }

        // OpDecorate %id BuiltIn <value>
        // Layout: [op|wc] [target_id] [decoration] [built_in_value]
        if (op == k_op_decorate && wc >= 4)
        {
            const uint32_t target_id = bytecode[i + 1];
            const uint32_t decoration = bytecode[i + 2];
            if (decoration == k_decoration_built_in)
            {
                const uint32_t built_in = bytecode[i + 3];
                switch (built_in)
                {
                    case BuiltInVertexIndex:
                    {
                        // Replace with OpenGL mapping instead of Vulkan one
                        id_vertex_index = target_id;
                        bytecode[i + 3] = k_built_in_vertex_id;
                        break;
                    }
                    case BuiltInInstanceIndex:
                    {
                        // Replace with OpenGL mapping instead of Vulkan one
                        id_instance_index = target_id;
                        bytecode[i + 3] = k_built_in_instance_id;
                        break;
                    }
                    case BuiltInBaseVertex:
                    {
                        // Not used in OpenGL, just remove it
                        id_base_vertex = target_id;
                        ReplaceWithNop(&bytecode[i], wc);
                        break;
                    }
                    case BuiltInBaseInstance:
                    {
                        // Not used in OpenGL, just remove it
                        id_base_instance = target_id;
                        ReplaceWithNop(&bytecode[i], wc);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        i += wc;
    }

    // If we found nothing to patch, return early — module is fine as-is.
    if (id_instance_index == 0 && id_vertex_index == 0 && id_base_instance == 0 && id_base_vertex == 0)
    {
        return;
    }

    // ── Pass 2: Find OpLoad results for base variables ──────────────────────
    // We need to know the SSA IDs of loads from BaseInstance / BaseVertex
    // so we can find the OpISub that uses them.

    uint32_t load_result_base_instance = 0;
    uint32_t load_result_base_vertex = 0;
    uint32_t load_result_instance_idx = 0;
    uint32_t load_result_vertex_idx = 0;

    for (uint32_t i = k_header_words; i < bytecode.GetSize();)
    {
        const uint32_t wc = GetWordCount(bytecode[i]);
        const uint32_t op = GetOpCode(bytecode[i]);
        if (wc == 0)
        {
            break;
        }

        // OpLoad %result_type %result_id %pointer
        if (op == OpLoad && wc >= 4)
        {
            uint32_t result_id = bytecode[i + 2];
            uint32_t pointer = bytecode[i + 3];

            if (pointer == id_base_instance)
                load_result_base_instance = result_id;
            if (pointer == id_base_vertex)
                load_result_base_vertex = result_id;
            if (pointer == id_instance_index)
                load_result_instance_idx = result_id;
            if (pointer == id_vertex_index)
                load_result_vertex_idx = result_id;
        }
        i += wc;
    }

    // ── Pass 3: Patch instructions ─────────────────────────────────────────
    // - Remove DrawParameters capability (not needed in OpenGL).
    // - NOP OpVariable declarations for BaseVertex / BaseInstance.
    // - NOP OpLoad instructions that load from BaseVertex / BaseInstance.
    // - Replace OpISub that subtracts a base load with OpCopyObject of the
    //   index operand, effectively making result = index - 0.

    for (uint32_t i = k_header_words; i < bytecode.GetSize();)
    {
        const uint32_t wc = GetWordCount(bytecode[i]);
        const uint32_t op = GetOpCode(bytecode[i]);
        if (wc == 0)
        {
            break;
        }

        // Remove DrawParameters capability.
        if (op == k_op_capability && wc >= 2 && bytecode[i + 1] == k_capability_draw_parameters)
        {
            ReplaceWithNop(&bytecode[i], wc);
        }

        // Remove SPV_KHR_shader_draw_parameters extension.
        if (op == k_op_extension && wc >= 2 && CompareWithString(&bytecode[i + 1], wc - 1, "SPV_KHR_shader_draw_parameters"))
        {
            ReplaceWithNop(&bytecode[i], wc);
        }

        // NOP OpVariable for base builtins.
        if (op == OpVariable && wc >= 4)
        {
            const uint32_t var_id = bytecode[i + 2];
            if ((id_base_vertex != 0 && var_id == id_base_vertex) || (id_base_instance != 0 && var_id == id_base_instance))
            {
                ReplaceWithNop(&bytecode[i], wc);
            }
        }

        // NOP OpLoad from base builtins.
        if (op == OpLoad && wc >= 4)
        {
            const uint32_t pointer = bytecode[i + 3];
            if ((id_base_vertex != 0 && pointer == id_base_vertex) || (id_base_instance != 0 && pointer == id_base_instance))
            {
                ReplaceWithNop(&bytecode[i], wc);
            }
        }

        // Replace OpISub that uses a base load with OpCopyObject of the index operand.
        // OpISub layout: [op|wc] [result_type] [result_id] [operand1] [operand2]
        // Result = operand1 - operand2
        if (op == OpISub && wc == 5)
        {
            const uint32_t op1 = bytecode[i + 3];
            const uint32_t op2 = bytecode[i + 4];

            uint32_t keep = 0;
            if ((load_result_base_vertex != 0 && op2 == load_result_base_vertex) ||
                (load_result_base_instance != 0 && op2 == load_result_base_instance))
            {
                keep = op1;
            }
            else if ((load_result_base_vertex != 0 && op1 == load_result_base_vertex) ||
                     (load_result_base_instance != 0 && op1 == load_result_base_instance))
            {
                keep = op2;
            }

            if (keep != 0)
            {
                // OpCopyObject is 4 words: [op|wc] [result_type] [result_id] [operand]
                bytecode[i] = CreateOp(4, OpCopyObject);
                bytecode[i + 3] = keep;
                bytecode[i + 4] = CreateOp(1, k_op_nop);
            }
        }

        i += wc;
    }
}