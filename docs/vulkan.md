# Descriptors

Descriptor is a reference or a handle that tells the GPU how to access some resource. Each descriptor desribes one
resource binding. It can be a sampled image, sampler, combined image-sampler, uniform buffer, storage buffer, storage image.
Each descriptor stores information about one of these resources and is used to access its contents.

Each shader will have a set of descriptors that it needs to be specified for it to run properly. Each descriptor set
has a layout, which defines what type of descriptor is bound to what location, and actual descriptor content, which
means specifying what actual textures or buffers descriptors reference. In general shaders in Vulkan can have up to
4 different descriptor sets bound.

Descriptor pools are allocators from which we allocate descriptor sets. When creating them we specify how many descriptor
sets we can allocate from this descriptor pool and how many descriptors of each type to have in the pool.

Once descriptor set is allocated from the pool it needs to be filled with data. Here we use VkWriteDescriptorSet structs
where each one describes one descriptor.

Promoted to the core in Vulkan 1.2, lets you create variable-size arrays of descriptors and index into them dynamically at
shader runtime (bindless). It also allows you to modify descriptors in this array, so that you don't have to constantly
rebind descriptor sets per draw call.

When creating a descriptor set layout, this layout will have for example one binding which is array of textures and at
runtime we 