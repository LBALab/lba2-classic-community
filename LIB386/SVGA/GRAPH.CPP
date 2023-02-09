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

// =============================================================================
#ifdef __cplusplus
}
#endif
