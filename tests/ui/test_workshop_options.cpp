#include "doctest.h"
#include "GameLogic/Items/WorkshopOptions.h"

#include <array>
#include <vector>

using namespace GameLogic::Items::Workshop;

namespace
{
std::vector<int> ChoicesOf(const Options& options)
{
    return {options.Choices().begin(), options.Choices().end()};
}
} // namespace

TEST_CASE("A plain item offers the Luck and every excellent option, in the tooltip's order")
{
    const Options options(std::array{Item{9}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.ExcellentCount() == 0);
    CHECK(ChoicesOf(options) == std::vector{LuckChoice, 6, 5, 4, 3, 2, 1});
}

TEST_CASE("The options the item has are left out")
{
    const Options options(std::array{Item{11, 0b100001, true}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.ExcellentCount() == 2);
    CHECK(ChoicesOf(options) == std::vector{5, 4, 3, 2});
}

TEST_CASE("An item with three excellent options offers only the Luck, then nothing")
{
    std::array items{Item{0, 0b010101}};
    const Options withoutLuck(items);
    CHECK(withoutLuck.GetStatus() == Status::Ready);
    CHECK(ChoicesOf(withoutLuck) == std::vector{LuckChoice});

    items[0].luck = true;
    const Options withLuck(items);
    CHECK(withLuck.GetStatus() == Status::NoOptionLeft);
    CHECK(withLuck.Choices().empty());
}

TEST_CASE("No item, two items, an ancient, a socket item or an item above +11 has no options")
{
    CHECK(Options(std::span<const Item>{}).GetStatus() == Status::NoItem);
    CHECK(Options(std::array{Item{}, Item{}}).GetStatus() == Status::TooManyItems);

    Item ancient;
    ancient.ancient = true;
    CHECK(Options(std::array{ancient}).GetStatus() == Status::Ancient);

    Item socket;
    socket.socket = true;
    CHECK(Options(std::array{socket}).GetStatus() == Status::Socket);

    const Options high(std::array{Item{MaximumItemLevel + 1}});
    CHECK(high.GetStatus() == Status::LevelTooHigh);
    CHECK(high.Choices().empty());
}

TEST_CASE("A +11 item still gets options, and flags above the six excellent options don't count")
{
    const Options options(std::array{Item{MaximumItemLevel, 0b11000001}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.ExcellentCount() == 1);
    CHECK(ChoicesOf(options) == std::vector{LuckChoice, 6, 5, 4, 3, 2});
}

TEST_CASE("An ancient comes before a socket, and a socket before the level")
{
    Item item{MaximumItemLevel + 1};
    item.ancient = true;
    item.socket = true;
    CHECK(Options(std::array{item}).GetStatus() == Status::Ancient);
    item.ancient = false;
    CHECK(Options(std::array{item}).GetStatus() == Status::Socket);
}

TEST_CASE("The Luck has the first recipe, an excellent option the one after the item's count")
{
    CHECK(GetRecipeIndex(LuckChoice, 2) == 0);
    CHECK(GetRecipeIndex(6, 0) == 1);
    CHECK(GetRecipeIndex(1, 1) == 2);
    CHECK(GetRecipeIndex(3, 2) == 3);
}
