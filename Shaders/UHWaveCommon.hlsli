// helper functions header for wave operations
uint GetWaveFirstActiveLane(bool bCondition)
{
    uint4 Mask = WaveActiveBallot(bCondition);

    if (Mask.x > 0)
    {
        return firstbitlow(Mask.x);
    }
    else if (Mask.y > 0)
    {
        return firstbitlow(Mask.y) + 32;
    }
    else if (Mask.z > 0)
    {
        return firstbitlow(Mask.z) + 64;
    }
    else if (Mask.w > 0)
    {
        return firstbitlow(Mask.w) + 96;
    }

    return 0xFFFFFFFF;
}