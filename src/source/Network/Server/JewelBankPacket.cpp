#include "Network/Server/JewelBankPacket.h"

#include <cstring>

namespace Network::JewelBank
{
bool Read(const std::uint8_t* data, std::size_t size, GameLogic::Items::JewelBankContents& contents, bool& open)
{
    if (data == nullptr || size < sizeof(ContentsHeader))
        return false;

    ContentsHeader header;
    std::memcpy(&header, data, sizeof(ContentsHeader));
    if (size < sizeof(ContentsHeader) + header.jewelCount * sizeof(ContentsJewel))
        return false;

    open = (header.flags & Open) != 0;
    contents.depositFeePerJewel = header.depositFeePerJewel;
    contents.withdrawalFeePerJewel = header.withdrawalFeePerJewel;
    contents.jewels.clear();
    contents.jewels.reserve(header.jewelCount);
    for (std::size_t i = 0; i < header.jewelCount; ++i)
    {
        ContentsJewel jewel;
        std::memcpy(&jewel, data + sizeof(ContentsHeader) + i * sizeof(ContentsJewel), sizeof(ContentsJewel));
        contents.jewels.push_back({jewel.group, jewel.number, jewel.amount});
    }
    return true;
}

Request MakeRequest(GameLogic::Items::JewelBankAction action, int jewelIndex, int amount)
{
    Request request{};
    request.type = 0xC1;
    request.size = sizeof(Request);
    request.headCode = HeadCode;
    request.subCode = RequestSubCode;
    request.action = static_cast<std::uint8_t>(action);
    request.jewelIndex = static_cast<std::uint8_t>(jewelIndex);
    request.amount = static_cast<std::uint8_t>(amount);
    return request;
}
} // namespace Network::JewelBank
