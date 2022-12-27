#pragma once

#include <vector>

struct IntPoint
{
    int X = 0;
    int Y = 0;
};

class AtlasPacker
{
public:
    struct RectIn
    {
        IntPoint Size;
        uintptr_t UserData;
    };

    struct RectOut
    {
        IntPoint BottomLeft;
        IntPoint Size;
        uintptr_t UserData = 0;
    };

    enum class SortCriteria
    {
        Height,
        Width,
        Area,
        PathologicalMultiplier  // max(w, h) / min(w, h) * w * h
    };

public:
    AtlasPacker(const int AtlasWidth, const int AtlasHeight, SortCriteria SortCrit);

    std::vector<RectOut> Pack(std::vector<RectIn>& InRects);

    static void Test();

private:
    static float PathologicMult(const RectIn& R);

    // Returns true if A is greater than B
    bool GreaterThan(const RectIn& A, const RectIn& B) const;
    bool GreaterThan(const RectOut& A, const RectOut& B);

private:
    IntPoint m_AtlasSize;
    SortCriteria m_SortCriteria;
    std::vector<RectOut> m_FreeSlots;
};
