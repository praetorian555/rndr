#include <catch2/catch2.hpp>

#include "../include/rndr/renderers/renderer-base.hpp"

class TestRenderer : public Rndr::RendererBase
{
public:
    TestRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc) : RendererBase(name, desc) {}
    bool Render() override { m_value++; return true; }

    int32_t m_value = 0;
};

TEST_CASE("Tests for renderer manager", "[renderer-manager]")
{
    SECTION("Add single renderer")
    {
        Rndr::RendererManager manager;
        auto renderer = new TestRenderer("Test", Rndr::RendererBaseDesc{});
        REQUIRE(manager.AddRenderer(renderer));
        REQUIRE(manager.GetRendererIndex("Test") == 0);
        REQUIRE(manager.Render());
        REQUIRE(renderer->m_value == 1);

        SECTION("Add second renderer to the back")
        {
            auto renderer2 = new TestRenderer("Test2", Rndr::RendererBaseDesc{});
            REQUIRE(manager.AddRenderer(renderer2));
            REQUIRE(manager.GetRendererIndex("Test2") == 1);
            REQUIRE(manager.Render());
            REQUIRE(renderer->m_value == 2);
            REQUIRE(renderer2->m_value == 1);

            SECTION("Add third renderer in the middle")
            {
                auto renderer3 = new TestRenderer("Test3", Rndr::RendererBaseDesc{});
                REQUIRE(manager.AddRendererAfter(renderer3, "Test"));
                REQUIRE(manager.GetRendererIndex("Test3") == 1);
                REQUIRE(manager.GetRendererIndex("Test") == 0);
                REQUIRE(manager.GetRendererIndex("Test2") == 2);
                REQUIRE(manager.Render());
                REQUIRE(renderer->m_value == 3);
                REQUIRE(renderer2->m_value == 2);
                REQUIRE(renderer3->m_value == 1);

                SECTION("Remove renderer by name")
                {
                    REQUIRE(manager.RemoveRenderer("Test"));
                    REQUIRE(manager.GetRendererIndex("Test") == -1);
                    REQUIRE(manager.GetRendererIndex("Test2") == 1);
                    REQUIRE(manager.GetRendererIndex("Test3") == 0);
                    REQUIRE(manager.Render());
                    REQUIRE(renderer->m_value == 3);
                    REQUIRE(renderer2->m_value == 3);
                    REQUIRE(renderer3->m_value == 2);
                }
            }

            SECTION("Add third renderer to the front")
            {
                auto renderer3 = new TestRenderer("Test3", Rndr::RendererBaseDesc{});
                REQUIRE(manager.AddRendererBefore(renderer3, "Test"));
                REQUIRE(manager.GetRendererIndex("Test3") == 0);
                REQUIRE(manager.GetRendererIndex("Test") == 1);
                REQUIRE(manager.GetRendererIndex("Test2") == 2);
                REQUIRE(manager.Render());
                REQUIRE(renderer->m_value == 3);
                REQUIRE(renderer2->m_value == 2);
                REQUIRE(renderer3->m_value == 1);

                SECTION("Remove renderer by value")
                {
                    REQUIRE(manager.RemoveRenderer(renderer));
                    REQUIRE(manager.GetRendererIndex("Test") == -1);
                    REQUIRE(manager.GetRendererIndex("Test2") == 1);
                    REQUIRE(manager.GetRendererIndex("Test3") == 0);
                    REQUIRE(manager.Render());
                    REQUIRE(renderer->m_value == 3);
                    REQUIRE(renderer2->m_value == 3);
                    REQUIRE(renderer3->m_value == 2);
                }
            }
        }
    }
}