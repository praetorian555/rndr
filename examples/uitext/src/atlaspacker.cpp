#include "atlaspacker.h"

#include <algorithm>
#include <cassert>

AtlasPacker::AtlasPacker(const int AtlasWidth, const int AtlasHeight, SortCriteria SortCrit)
    : m_AtlasSize{AtlasWidth, AtlasHeight}, m_SortCriteria(SortCrit)
{
    // Initialize array of free slots and populate it by one slot which has the size of the
    // whole atlas
    RectOut StartSlot{IntPoint{0, 0}, m_AtlasSize, 0};
    m_FreeSlots.push_back(StartSlot);
}

float AtlasPacker::PathologicMult(const RectIn& R)
{
    return std::max(R.Size.X, R.Size.Y) / std::min(R.Size.X, R.Size.Y) * R.Size.X * R.Size.Y;
}

// Returns true if A is greater than B
bool AtlasPacker::GreaterThan(const RectIn& A, const RectIn& B) const
{
    switch (m_SortCriteria)
    {
        case SortCriteria::Height:
        {
            return A.Size.Y > B.Size.Y;
        }
        case SortCriteria::Width:
        {
            return A.Size.X > B.Size.X;
        }
        case SortCriteria::Area:
        {
            return A.Size.X * A.Size.Y > B.Size.X * B.Size.Y;
        }
        case SortCriteria::PathologicalMultiplier:
        {
            return PathologicMult(A) > PathologicMult(B);
        }
    }
    assert(false);
    return true;
}

bool AtlasPacker::GreaterThan(const RectOut& A, const RectOut& B)
{
    RectIn AA{A.Size};
    RectIn BB{B.Size};
    return GreaterThan(AA, BB);
}

std::vector<AtlasPacker::RectOut> AtlasPacker::Pack(std::vector<RectIn>& InRects)
{
    // Sort input rectangles based on the sorting criteria
    std::sort(InRects.begin(), InRects.end(),
              [this](const RectIn& A, const RectIn& B) { return GreaterThan(A, B); });

    std::vector<RectOut> OutRects;
    for (const RectIn& InRect : InRects)
    {
        // No more free space left
        if (m_FreeSlots.empty())
        {
            break;
        }

        // Go through free space slots, from smaller to bigger
        for (int i = m_FreeSlots.size() - 1; i >= 0; i--)
        {
            RectOut& Slot = m_FreeSlots[i];

            // This slot is too small, skip it
            if (Slot.Size.X < InRect.Size.X || Slot.Size.Y < InRect.Size.Y)
            {
                continue;
            }

            // We found a free slot for the input rect, add info the output rect list
            RectOut OutRect;
            OutRect.BottomLeft = Slot.BottomLeft;
            OutRect.Size = InRect.Size;
            OutRect.UserData = InRect.UserData;
            OutRects.push_back(OutRect);

            const int DiffX = Slot.Size.X - InRect.Size.X;
            const int DiffY = Slot.Size.Y - InRect.Size.Y;

            RectOut SmallerSlot;
            SmallerSlot.BottomLeft = IntPoint{Slot.BottomLeft.X + InRect.Size.X, Slot.BottomLeft.Y};
            SmallerSlot.Size = IntPoint{Slot.Size.X - InRect.Size.X, InRect.Size.Y};

            RectOut BiggerSlot;
            BiggerSlot.BottomLeft = IntPoint{Slot.BottomLeft.X, Slot.BottomLeft.Y + InRect.Size.Y};
            BiggerSlot.Size = IntPoint{Slot.Size.X, Slot.Size.Y - InRect.Size.Y};

            if (GreaterThan(SmallerSlot, BiggerSlot))
            {
                std::swap(SmallerSlot, BiggerSlot);
            }

            // Move the current slot to the back and remove it from the list
            std::swap(Slot, m_FreeSlots.back());
            m_FreeSlots.pop_back();

            if (BiggerSlot.Size.X != 0 && BiggerSlot.Size.Y != 0)
            {
                m_FreeSlots.push_back(BiggerSlot);
            }
            if (SmallerSlot.Size.X != 0 && SmallerSlot.Size.Y != 0)
            {
                m_FreeSlots.push_back(SmallerSlot);
            }

            break;
        }
    }

    return OutRects;
}

void AtlasPacker::Test()
{
    {
        AtlasPacker P(100, 100, SortCriteria::Height);
        std::vector<RectIn> InRects;
        InRects.push_back(RectIn{{10, 20}, 0});
        InRects.push_back(RectIn{{5, 5}, 1});
        InRects.push_back(RectIn{{20, 10}, 2});
        InRects.push_back(RectIn{{30, 30}, 3});

        std::vector<RectOut> OutRects = P.Pack(InRects);
        assert(OutRects.size() == 4);

        assert(OutRects[0].BottomLeft.X == 0);
        assert(OutRects[0].BottomLeft.Y == 0);
        assert(OutRects[0].UserData == 3);

        assert(OutRects[1].BottomLeft.X == 30);
        assert(OutRects[1].BottomLeft.Y == 0);
        assert(OutRects[1].UserData == 0);

        assert(OutRects[2].BottomLeft.X == 30);
        assert(OutRects[2].BottomLeft.Y == 20);
        assert(OutRects[2].UserData == 2);

        assert(OutRects[3].BottomLeft.X == 50);
        assert(OutRects[3].BottomLeft.Y == 20);
        assert(OutRects[3].UserData == 1);
    }

    {
        AtlasPacker P(100, 100, SortCriteria::Height);
        std::vector<RectIn> InRects;
        InRects.resize(16);
        for (int i = 0; i < 16; i++)
        {
            InRects[i] = {{25, 25}, 0};
        }

        std::vector<RectOut> OutRects = P.Pack(InRects);
        assert(OutRects.size() == 16);

        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                int Index = i * 4 + j;
                assert(OutRects[Index].BottomLeft.X == j * 25);
                assert(OutRects[Index].BottomLeft.Y == i * 25);
            }
        }
    }
}
