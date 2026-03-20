/* Test: ObjectStoreFrame — store current animation frame to circular buffer */
#include "test_harness.h"
#include <ANIM/STOFRAME.H>
#include <ANIM/ANIM.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <OBJECT/AFF_OBJ.H>
#include "anim_test_fixture.h"
#include <string.h>

/* ObjectStoreFrame ASM uses Watcom convention: ebx = obj */
extern "C" void asm_ObjectStoreFrame(T_OBJ_3D *obj);
static inline void call_asm_ObjectStoreFrame(T_OBJ_3D *obj) {
    __asm__ __volatile__(
        "call asm_ObjectStoreFrame"
        : "+b"(obj)
        :
        : "eax", "ecx", "edx", "edi", "esi", "memory", "cc");
}

static U8 test_anim[512];

static void setup(T_OBJ_3D *obj) {
    build_anim_header(test_anim, 2, 3, 0, 100);
    set_anim_group(test_anim, 3, 0, 1, 0, 100, 200, 300);
    set_anim_group(test_anim, 3, 0, 2, 0, 400, 500, 600);
    init_test_obj(obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(obj, test_anim);
}

static void test_cpp_store(void) {
    T_OBJ_3D obj;
    setup(&obj);
    U8 *ptr_before = (U8 *)PtrLib3DBufferAnim;
    ObjectStoreFrame(&obj);
    /* PtrLib3DBufferAnim should have advanced */
    ASSERT_TRUE((U8 *)PtrLib3DBufferAnim > ptr_before);
    /* LastOfsIsPtr should be set to 1 */
    ASSERT_EQ_UINT(1, obj.LastOfsIsPtr);
    /* LastFrame should be -1 (stored frame, not an anim index) */
    ASSERT_EQ_INT(-1, obj.LastFrame);
}

static void test_cpp_stored_data(void) {
    T_OBJ_3D obj;
    setup(&obj);
    U32 *stored = (U32 *)PtrLib3DBufferAnim;
    ObjectStoreFrame(&obj);
    /* After 16-byte header (4 zero dwords), stored data = CurrentFrame */
    S16 *storedGroups = (S16 *)(stored + 4);
    ASSERT_EQ_INT(100, storedGroups[1]); /* group 1 Alpha */
    ASSERT_EQ_INT(200, storedGroups[2]); /* group 1 Beta */
    ASSERT_EQ_INT(300, storedGroups[3]); /* group 1 Gamma */
}

static void test_asm_equiv(void) {
    T_OBJ_3D cpp_obj, asm_obj;
    U8 cpp_buffer[TEST_ANIM_BUFFER_SIZE];
    U8 asm_buffer[TEST_ANIM_BUFFER_SIZE];

    TransFctAnim = NULL;

    /* CPP */
    setup(&cpp_obj);
    ObjectStoreFrame(&cpp_obj);
    U32 cpp_advance = (U8 *)PtrLib3DBufferAnim - g_anim_buffer;
    memcpy(cpp_buffer, g_anim_buffer, TEST_ANIM_BUFFER_SIZE);

    /* ASM */
    setup(&asm_obj);
    call_asm_ObjectStoreFrame(&asm_obj);
    U32 asm_advance = (U8 *)PtrLib3DBufferAnim - g_anim_buffer;
    memcpy(asm_buffer, g_anim_buffer, TEST_ANIM_BUFFER_SIZE);

    ASSERT_EQ_UINT(cpp_advance, asm_advance);
    ASSERT_ASM_CPP_MEM_EQ(asm_buffer, cpp_buffer, cpp_advance,
                          "ObjectStoreFrame buffer");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)&asm_obj, (U8 *)&cpp_obj, sizeof(T_OBJ_3D),
                          "ObjectStoreFrame object state");
}

int main(void) {
    RUN_TEST(test_cpp_store);
    RUN_TEST(test_cpp_stored_data);
    RUN_TEST(test_asm_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
