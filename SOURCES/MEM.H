/*

	Gestion de la mémoire LBA2


*/

#define NB_MEM_ELTS NbTabIndex(ListMem)

enum {
    MEM_PTR_ZBUFFER = 0,
    MEM_SCREEN_AUX,
    MEM_BUFSPEAK,
    MEM_SCREEN,
    MEM_BUFFER_MASK_BRICK,
    MEM_BUF_MAP,
    MEM_TAB_BLOCK,
    MEM_LIST_FLOW_DOTS,
    MEM_PTR_XPL_PALETTE,
    MEM_IMAGE_STAGING /* 640x480 staging for stride-aware image loaders, see IMAGE_LOAD.H */
};

typedef struct {
    void **AdrPtr;
    S32 Size;
} T_MEM;

extern T_MEM ListMem[];

extern S32 SizeOfMemAllocated; // calculée dynamiquement
extern U8 *MainBuffer;

extern void AdjustLba2Mem(void);
extern void InitMainBuffer(void);
extern void InitMemory(void);
extern void Mem_ConfigureScreenBuffers(U32 resX, U32 resY);
