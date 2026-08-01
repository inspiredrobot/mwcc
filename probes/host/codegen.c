typedef unsigned char Boolean;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef unsigned long long UInt64;

typedef struct FourWords {
    UInt32 first;
    UInt32 second;
    UInt32 third;
    UInt32 fourth;
} FourWords;

static const UInt16 bit_masks[16] = {
    0x0001, 0x0002, 0x0004, 0x0008,
    0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800,
    0x1000, 0x2000, 0x4000, 0x8000,
};

int absolute_int(int value)
{
    if (value < 0) {
        return -value;
    }
    return value;
}

int short_predecessor(short value)
{
    if (value != 0) {
        return value - 1;
    }
    return -1;
}

Boolean initialize_four_words(FourWords* value, UInt32 fourth)
{
    value->first = 0;
    value->second = 0;
    value->third = 0;
    value->fourth = fourth;
    return 1;
}

UInt64 xor_64(UInt64 left, UInt64 right)
{
    return left ^ right;
}

UInt16 test_bit(const UInt16* words, short bit)
{
    return bit_masks[bit & 15] & words[bit >> 4];
}
