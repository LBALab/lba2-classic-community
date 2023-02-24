#include <SVGA/GRAPH.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
S32 GetDxDyGraph(S32 numgraph, S32 *x, S32 *y, const void *bankgraph) {
  const S32 *graphMem = (const S32 *)bankgraph;
  const U8 *graphBytes = (const U8 *)bankgraph;

  // Don't know if the value is really a signed int, but works for now
  S32 offset = graphMem[numgraph];

  *x = (graphBytes[offset] & 0xFF);
  *y = (graphBytes[offset + 1] & 0xFF);

  return 0; // TODO: Remove, make void
}

#include <stdio.h>

S32 AffGraph(S32 numgraph, S32 x, S32 y, const void *bankgraph)
{
  // TODO: Implement
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

S32 GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1,
                const void *bankgraph)
{
  // TODO: Implement
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
