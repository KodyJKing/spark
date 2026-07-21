typedef unsigned char   undefined;

typedef unsigned char    bool;
typedef unsigned char    byte;
typedef unsigned int    dword;
typedef unsigned long long    GUID;
typedef pointer32 ImageBaseOffset32;

typedef pointer64 ImageBaseOffset64;

typedef long long    longlong;
typedef unsigned long long    qword;
typedef unsigned char    uchar;
typedef unsigned int    uint;
typedef unsigned long    ulong;
typedef unsigned long long    ulonglong;
typedef unsigned char    undefined1;
typedef unsigned short    undefined2;
typedef unsigned int    undefined3;
typedef unsigned int    undefined4;
typedef unsigned long long    undefined5;
typedef unsigned long long    undefined8;
typedef unsigned short    ushort;
typedef unsigned short    wchar16;
typedef short    wchar_t;
typedef unsigned short    word;
typedef struct RaycastResult RaycastResult, *PRaycastResult;

typedef enum HitType {
    HitType_Map=2,
    HitType_Entity=3,
    HitType_Nothing=65535
} HitType;

typedef struct Vec3 Vec3, *PVec3;

typedef ushort uint16_t;

typedef struct DatumHandle DatumHandle, *PDatumHandle;

typedef struct DatumHandle EntityHandle;

struct DatumHandle {
    uint16_t index;
    uint16_t salt;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct RaycastResult {
    enum HitType hitType;
    undefined field1_0x2;
    undefined field2_0x3;
    undefined field3_0x4;
    undefined field4_0x5;
    undefined field5_0x6;
    undefined field6_0x7;
    undefined field7_0x8;
    undefined field8_0x9;
    undefined field9_0xa;
    undefined field10_0xb;
    short field11_0xc;
    undefined field12_0xe;
    undefined field13_0xf;
    undefined field14_0x10;
    undefined field15_0x11;
    undefined field16_0x12;
    undefined field17_0x13;
    undefined field18_0x14;
    undefined field19_0x15;
    undefined field20_0x16;
    undefined field21_0x17;
    struct Vec3 pos;
    struct Vec3 normal;
    undefined field24_0x30;
    undefined field25_0x31;
    undefined field26_0x32;
    undefined field27_0x33;
    uint16_t field28_0x34;
    byte field29_0x36;
    undefined1 field30_0x37;
    EntityHandle entityHandle;
};

