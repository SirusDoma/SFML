#include <SFML/Graphics/RenderTarget.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

TEST_CASE("[Graphics] sf::RenderTarget")
{
    SECTION("Type traits")
    {
        STATIC_CHECK(!std::is_constructible_v<sf::RenderTarget>);
        STATIC_CHECK(!std::is_copy_constructible_v<sf::RenderTarget>);
        STATIC_CHECK(!std::is_copy_assignable_v<sf::RenderTarget>);
        STATIC_CHECK(!std::is_nothrow_move_constructible_v<sf::RenderTarget>);
        STATIC_CHECK(std::is_nothrow_move_assignable_v<sf::RenderTarget>);
        STATIC_CHECK(std::is_abstract_v<sf::RenderTarget>);
        STATIC_CHECK(std::has_virtual_destructor_v<sf::RenderTarget>);
    }
}
