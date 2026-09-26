/* Embedded SMAA 1x shader core (SMAA.hlsl), verbatim from the reference
 * implementation (libretro slang-shaders port).  MIT license - copyright
 * Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro,
 * Diego Gutierrez.  Consumed by the three SMAA pass shaders in
 * RENDER_GL.CPP with SMAA_GLSL_3 defined on GLES3 contexts. */
#ifndef SMAA_HLSL_H
#define SMAA_HLSL_H

#define SMAA_HLSL_SRC \
"/**\n" \
" * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)\n" \
" * Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)\n" \
" * Copyright (C) 2013 Belen Masia (bmasia@unizar.es)\n" \
" * Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)\n" \
" * Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)\n" \
" *\n" \
" * Permission is hereby granted, free of charge, to any person obtaining a copy\n" \
" * this software and associated documentation files (the \"Software\"), to deal in\n" \
" * the Software without restriction, including without limitation the rights to\n" \
" * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies\n" \
" * of the Software, and to permit persons to whom the Software is furnished to\n" \
" * do so, subject to the following conditions:\n" \
" *\n" \
" * The above copyright notice and this permission notice shall be included in\n" \
" * all copies or substantial portions of the Software. As clarification, there\n" \
" * is no requirement that the copyright notice and permission be included in\n" \
" * binary distributions of the Software.\n" \
" *\n" \
" * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n" \
" * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n" \
" * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n" \
" * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n" \
" * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n" \
" * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n" \
" * SOFTWARE.\n" \
" */\n" \
"\n" \
"\n" \
"/**\n" \
" *                  _______  ___  ___       ___           ___\n" \
" *                 /       ||   \\/   |     /   \\         /   \\\n" \
" *                |   (---- |  \\  /  |    /  ^  \\       /  ^  \\\n" \
" *                 \\   \\    |  |\\/|  |   /  /_\\  \\     /  /_\\  \\\n" \
" *              ----)   |   |  |  |  |  /  _____  \\   /  _____  \\\n" \
" *             |_______/    |__|  |__| /__/     \\__\\ /__/     \\__\\\n" \
" * \n" \
" *                               E N H A N C E D\n" \
" *       S U B P I X E L   M O R P H O L O G I C A L   A N T I A L I A S I N G\n" \
" *\n" \
" *                         http://www.iryoku.com/smaa/\n" \
" *\n" \
" * Hi, welcome aboard!\n" \
" * \n" \
" * Here you'll find instructions to get the shader up and running as fast as\n" \
" * possible.\n" \
" *\n" \
" * IMPORTANTE NOTICE: when updating, remember to update both this file and the\n" \
" * precomputed textures! They may change from version to version.\n" \
" *\n" \
" * The shader has three passes, chained together as follows:\n" \
" *\n" \
" *                           |input|------------------?\n" \
" *                              v                     |\n" \
" *                    [ SMAA*EdgeDetection ]          |\n" \
" *                              v                     |\n" \
" *                          |edgesTex|                |\n" \
" *                              v                     |\n" \
" *              [ SMAABlendingWeightCalculation ]     |\n" \
" *                              v                     |\n" \
" *                          |blendTex|                |\n" \
" *                              v                     |\n" \
" *                [ SMAANeighborhoodBlending ] <------?\n" \
" *                              v\n" \
" *                           |output|\n" \
" *\n" \
" * Note that each [pass] has its own vertex and pixel shader. Remember to use\n" \
" * oversized triangles instead of quads to avoid overshading along the\n" \
" * diagonal.\n" \
" *\n" \
" * You've three edge detection methods to choose from: luma, color or depth.\n" \
" * They represent different quality/performance and anti-aliasing/sharpness\n" \
" * tradeoffs, so our recommendation is for you to choose the one that best\n" \
" * suits your particular scenario:\n" \
" *\n" \
" * - Depth edge detection is usually the fastest but it may miss some edges.\n" \
" *\n" \
" * - Luma edge detection is usually more expensive than depth edge detection,\n" \
" *   but catches visible edges that depth edge detection can miss.\n" \
" *\n" \
" * - Color edge detection is usually the most expensive one but catches\n" \
" *   chroma-only edges.\n" \
" *\n" \
" * For quickstarters: just use luma edge detection.\n" \
" *\n" \
" * The general advice is to not rush the integration process and ensure each\n" \
" * step is done correctly (don't try to integrate SMAA T2x with predicated edge\n" \
" * detection from the start!). Ok then, let's go!\n" \
" *\n" \
" *  1. The first step is to create two RGBA temporal render targets for holding\n" \
" *     |edgesTex| and |blendTex|.\n" \
" *\n" \
" *     In DX10 or DX11, you can use a RG render target for the edges texture.\n" \
" *     In the case of NVIDIA GPUs, using RG render targets seems to actually be\n" \
" *     slower.\n" \
" *\n" \
" *     On the Xbox 360, you can use the same render target for resolving both\n" \
" *     |edgesTex| and |blendTex|, as they aren't needed simultaneously.\n" \
" *\n" \
" *  2. Both temporal render targets |edgesTex| and |blendTex| must be cleared\n" \
" *     each frame. Do not forget to clear the alpha channel!\n" \
" *\n" \
" *  3. The next step is loading the two supporting precalculated textures,\n" \
" *     'areaTex' and 'searchTex'. You'll find them in the 'Textures' folder as\n" \
" *     C++ headers, and also as regular DDS files. They'll be needed for the\n" \
" *     'SMAABlendingWeightCalculation' pass.\n" \
" *\n" \
" *     If you use the C++ headers, be sure to load them in the format specified\n" \
" *     inside of them.\n" \
" *\n" \
" *     You can also compress 'areaTex' and 'searchTex' using BC5 and BC4\n" \
" *     respectively, if you have that option in your content processor pipeline.\n" \
" *     When compressing then, you get a non-perceptible quality decrease, and a\n" \
" *     marginal performance increase.\n" \
" *\n" \
" *  4. All samplers must be set to linear filtering and clamp.\n" \
" *\n" \
" *     After you get the technique working, remember that 64-bit inputs have\n" \
" *     half-rate linear filtering on GCN.\n" \
" *\n" \
" *     If SMAA is applied to 64-bit color buffers, switching to point filtering\n" \
" *     when accesing them will increase the performance. Search for\n" \
" *     'SMAASamplePoint' to see which textures may benefit from point\n" \
" *     filtering, and where (which is basically the color input in the edge\n" \
" *     detection and resolve passes).\n" \
" *\n" \
" *  5. All texture reads and buffer writes must be non-sRGB, with the exception\n" \
" *     of the input read and the output write in\n" \
" *     'SMAANeighborhoodBlending' (and only in this pass!). If sRGB reads in\n" \
" *     this last pass are not possible, the technique will work anyway, but\n" \
" *     will perform antialiasing in gamma space.\n" \
" *\n" \
" *     IMPORTANT: for best results the input read for the color/luma edge \n" \
" *     detection should *NOT* be sRGB.\n" \
" *\n" \
" *  6. Before including SMAA.h you'll have to setup the render target metrics,\n" \
" *     the target and any optional configuration defines. Optionally you can\n" \
" *     use a preset.\n" \
" *\n" \
" *     You have the following targets available: \n" \
" *         SMAA_HLSL_3\n" \
" *         SMAA_HLSL_4\n" \
" *         SMAA_HLSL_4_1\n" \
" *         SMAA_GLSL_3 *\n" \
" *         SMAA_GLSL_4 *\n" \
" *\n" \
" *         * (See SMAA_INCLUDE_VS and SMAA_INCLUDE_PS below).\n" \
" *\n" \
" *     And four presets:\n" \
" *         SMAA_PRESET_LOW          (%60 of the quality)\n" \
" *         SMAA_PRESET_MEDIUM       (%80 of the quality)\n" \
" *         SMAA_PRESET_HIGH         (%95 of the quality)\n" \
" *         SMAA_PRESET_ULTRA        (%99 of the quality)\n" \
" *\n" \
" *     For example:\n" \
" *         #define SMAA_RT_METRICS float4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0)\n" \
" *         #define SMAA_HLSL_4\n" \
" *         #define SMAA_PRESET_HIGH\n" \
" *         #include \"SMAA.h\"\n" \
" *\n" \
" *     Note that SMAA_RT_METRICS doesn't need to be a macro, it can be a\n" \
" *     uniform variable. The code is designed to minimize the impact of not\n" \
" *     using a constant value, but it is still better to hardcode it.\n" \
" *\n" \
" *     Depending on how you encoded 'areaTex' and 'searchTex', you may have to\n" \
" *     add (and customize) the following defines before including SMAA.h:\n" \
" *          #define SMAA_AREATEX_SELECT(sample) sample.rg\n" \
" *          #define SMAA_SEARCHTEX_SELECT(sample) sample.r\n" \
" *\n" \
" *     If your engine is already using porting macros, you can define\n" \
" *     SMAA_CUSTOM_SL, and define the porting functions by yourself.\n" \
" *\n" \
" *  7. Then, you'll have to setup the passes as indicated in the scheme above.\n" \
" *     You can take a look into SMAA.fx, to see how we did it for our demo.\n" \
" *     Checkout the function wrappers, you may want to copy-paste them!\n" \
" *\n" \
" *  8. It's recommended to validate the produced |edgesTex| and |blendTex|.\n" \
" *     You can use a screenshot from your engine to compare the |edgesTex|\n" \
" *     and |blendTex| produced inside of the engine with the results obtained\n" \
" *     with the reference demo.\n" \
" *\n" \
" *  9. After you get the last pass to work, it's time to optimize. You'll have\n" \
" *     to initialize a stencil buffer in the first pass (discard is already in\n" \
" *     the code), then mask execution by using it the second pass. The last\n" \
" *     pass should be executed in all pixels.\n" \
" *\n" \
" *\n" \
" * After this point you can choose to enable predicated thresholding,\n" \
" * temporal supersampling and motion blur integration:\n" \
" *\n" \
" * a) If you want to use predicated thresholding, take a look into\n" \
" *    SMAA_PREDICATION; you'll need to pass an extra texture in the edge\n" \
" *    detection pass.\n" \
" *\n" \
" * b) If you want to enable temporal supersampling (SMAA T2x):\n" \
" *\n" \
" * 1. The first step is to render using subpixel jitters. I won't go into\n" \
" *    detail, but it's as simple as moving each vertex position in the\n" \
" *    vertex shader, you can check how we do it in our DX10 demo.\n" \
" *\n" \
" * 2. Then, you must setup the temporal resolve. You may want to take a look\n" \
" *    into SMAAResolve for resolving 2x modes. After you get it working, you'll\n" \
" *    probably see ghosting everywhere. But fear not, you can enable the\n" \
" *    CryENGINE temporal reprojection by setting the SMAA_REPROJECTION macro.\n" \
" *    Check out SMAA_DECODE_VELOCITY if your velocity buffer is encoded.\n" \
" *\n" \
" * 3. The next step is to apply SMAA to each subpixel jittered frame, just as\n" \
" *    done for 1x.\n" \
" *\n" \
" * 4. At this point you should already have something usable, but for best\n" \
" *    results the proper area textures must be set depending on current jitter.\n" \
" *    For this, the parameter 'subsampleIndices' of\n" \
" *    'SMAABlendingWeightCalculationPS' must be set as follows, for our T2x\n" \
" *    mode:\n" \
" *\n" \
" *    @SUBSAMPLE_INDICES\n" \
" *\n" \
" *    | S# |  Camera Jitter   |  subsampleIndices    |\n" \
" *    +----+------------------+---------------------+\n" \
" *    |  0 |  ( 0.25, -0.25)  |  float4(1, 1, 1, 0)  |\n" \
" *    |  1 |  (-0.25,  0.25)  |  float4(2, 2, 2, 0)  |\n" \
" *\n" \
" *    These jitter positions assume a bottom-to-top y axis. S# stands for the\n" \
" *    sample number.\n" \
" *\n" \
" * More information about temporal supersampling here:\n" \
" *    http://iryoku.com/aacourse/downloads/13-Anti-Aliasing-Methods-in-CryENGINE-3.pdf\n" \
" *\n" \
" * c) If you want to enable spatial multisampling (SMAA S2x):\n" \
" *\n" \
" * 1. The scene must be rendered using MSAA 2x. The MSAA 2x buffer must be\n" \
" *    created with:\n" \
" *      - DX10:     see below (*)\n" \
" *      - DX10.1:   D3D10_STANDARD_MULTISAMPLE_PATTERN or\n" \
" *      - DX11:     D3D11_STANDARD_MULTISAMPLE_PATTERN\n" \
" *\n" \
" *    This allows to ensure that the subsample order matches the table in\n" \
" *    @SUBSAMPLE_INDICES.\n" \
" *\n" \
" *    (*) In the case of DX10, we refer the reader to:\n" \
" *      - SMAA::detectMSAAOrder and\n" \
" *      - SMAA::msaaReorder\n" \
" *\n" \
" *    These functions allow to match the standard multisample patterns by\n" \
" *    detecting the subsample order for a specific GPU, and reordering\n" \
" *    them appropriately.\n" \
" *\n" \
" * 2. A shader must be run to output each subsample into a separate buffer\n" \
" *    (DX10 is required). You can use SMAASeparate for this purpose, or just do\n" \
" *    it in an existing pass (for example, in the tone mapping pass, which has\n" \
" *    the advantage of feeding tone mapped subsamples to SMAA, which will yield\n" \
" *    better results).\n" \
" *\n" \
" * 3. The full SMAA 1x pipeline must be run for each separated buffer, storing\n" \
" *    the results in the final buffer. The second run should alpha blend with\n" \
" *    the existing final buffer using a blending factor of 0.5.\n" \
" *    'subsampleIndices' must be adjusted as in the SMAA T2x case (see point\n" \
" *    b).\n" \
" *\n" \
" * d) If you want to enable temporal supersampling on top of SMAA S2x\n" \
" *    (which actually is SMAA 4x):\n" \
" *\n" \
" * 1. SMAA 4x consists on temporally jittering SMAA S2x, so the first step is\n" \
" *    to calculate SMAA S2x for current frame. In this case, 'subsampleIndices'\n" \
" *    must be set as follows:\n" \
" *\n" \
" *    | F# | S# |   Camera Jitter    |    Net Jitter     |   subsampleIndices   |\n" \
" *    +----+----+--------------------+-------------------+----------------------+\n" \
" *    |  0 |  0 |  ( 0.125,  0.125)  |  ( 0.375, -0.125) |  float4(5, 3, 1, 3)  |\n" \
" *    |  0 |  1 |  ( 0.125,  0.125)  |  (-0.125,  0.375) |  float4(4, 6, 2, 3)  |\n" \
" *    +----+----+--------------------+-------------------+----------------------+\n" \
" *    |  1 |  2 |  (-0.125, -0.125)  |  ( 0.125, -0.375) |  float4(3, 5, 1, 4)  |\n" \
" *    |  1 |  3 |  (-0.125, -0.125)  |  (-0.375,  0.125) |  float4(6, 4, 2, 4)  |\n" \
" *\n" \
" *    These jitter positions assume a bottom-to-top y axis. F# stands for the\n" \
" *    frame number. S# stands for the sample number.\n" \
" *\n" \
" * 2. After calculating SMAA S2x for current frame (with the new subsample\n" \
" *    indices), previous frame must be reprojected as in SMAA T2x mode (see\n" \
" *    point b).\n" \
" *\n" \
" * e) If motion blur is used, you may want to do the edge detection pass\n" \
" *    together with motion blur. This has two advantages:\n" \
" *\n" \
" * 1. Pixels under heavy motion can be omitted from the edge detection process.\n" \
" *    For these pixels we can just store \"no edge\", as motion blur will take\n" \
" *    care of them.\n" \
" * 2. The center pixel tap is reused.\n" \
" *\n" \
" * Note that in this case depth testing should be used instead of stenciling,\n" \
" * as we have to write all the pixels in the motion blur pass.\n" \
" *\n" \
" * That's it!\n" \
" */\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// SMAA Presets\n" \
"\n" \
"/**\n" \
" * Note that if you use one of these presets, the following configuration\n" \
" * macros will be ignored if set in the \"Configurable Defines\" section.\n" \
" */\n" \
"\n" \
"#if defined(SMAA_PRESET_LOW)\n" \
"#define SMAA_THRESHOLD 0.15\n" \
"#define SMAA_MAX_SEARCH_STEPS 4\n" \
"#define SMAA_DISABLE_DIAG_DETECTION\n" \
"#define SMAA_DISABLE_CORNER_DETECTION\n" \
"#elif defined(SMAA_PRESET_MEDIUM)\n" \
"#define SMAA_THRESHOLD 0.1\n" \
"#define SMAA_MAX_SEARCH_STEPS 8\n" \
"#define SMAA_DISABLE_DIAG_DETECTION\n" \
"#define SMAA_DISABLE_CORNER_DETECTION\n" \
"#elif defined(SMAA_PRESET_HIGH)\n" \
"#define SMAA_THRESHOLD 0.1\n" \
"#define SMAA_MAX_SEARCH_STEPS 16\n" \
"#define SMAA_MAX_SEARCH_STEPS_DIAG 8\n" \
"#define SMAA_CORNER_ROUNDING 25\n" \
"#elif defined(SMAA_PRESET_ULTRA)\n" \
"#define SMAA_THRESHOLD 0.05\n" \
"#define SMAA_MAX_SEARCH_STEPS 32\n" \
"#define SMAA_MAX_SEARCH_STEPS_DIAG 16\n" \
"#define SMAA_CORNER_ROUNDING 25\n" \
"#endif\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Configurable Defines\n" \
"\n" \
"/**\n" \
" * SMAA_THRESHOLD specifies the threshold or sensitivity to edges.\n" \
" * Lowering this value you will be able to detect more edges at the expense of\n" \
" * performance. \n" \
" *\n" \
" * Range: [0, 0.5]\n" \
" *   0.1 is a reasonable value, and allows to catch most visible edges.\n" \
" *   0.05 is a rather overkill value, that allows to catch 'em all.\n" \
" *\n" \
" *   If temporal supersampling is used, 0.2 could be a reasonable value, as low\n" \
" *   contrast edges are properly filtered by just 2x.\n" \
" */\n" \
"#ifndef SMAA_THRESHOLD\n" \
"#define SMAA_THRESHOLD 0.1\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * SMAA_DEPTH_THRESHOLD specifies the threshold for depth edge detection.\n" \
" * \n" \
" * Range: depends on the depth range of the scene.\n" \
" */\n" \
"#ifndef SMAA_DEPTH_THRESHOLD\n" \
"#define SMAA_DEPTH_THRESHOLD (0.1 * SMAA_THRESHOLD)\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * SMAA_MAX_SEARCH_STEPS specifies the maximum steps performed in the\n" \
" * horizontal/vertical pattern searches, at each side of the pixel.\n" \
" *\n" \
" * In number of pixels, it's actually the double. So the maximum line length\n" \
" * perfectly handled by, for example 16, is 64 (by perfectly, we meant that\n" \
" * longer lines won't look as good, but still antialiased).\n" \
" *\n" \
" * Range: [0, 112]\n" \
" */\n" \
"#ifndef SMAA_MAX_SEARCH_STEPS\n" \
"#define SMAA_MAX_SEARCH_STEPS 16\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * SMAA_MAX_SEARCH_STEPS_DIAG specifies the maximum steps performed in the\n" \
" * diagonal pattern searches, at each side of the pixel. In this case we jump\n" \
" * one pixel at time, instead of two.\n" \
" *\n" \
" * Range: [0, 20]\n" \
" *\n" \
" * On high-end machines it is cheap (between a 0.8x and 0.9x slower for 16 \n" \
" * steps), but it can have a significant impact on older machines.\n" \
" *\n" \
" * Define SMAA_DISABLE_DIAG_DETECTION to disable diagonal processing.\n" \
" */\n" \
"#ifndef SMAA_MAX_SEARCH_STEPS_DIAG\n" \
"#define SMAA_MAX_SEARCH_STEPS_DIAG 8\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * SMAA_CORNER_ROUNDING specifies how much sharp corners will be rounded.\n" \
" *\n" \
" * Range: [0, 100]\n" \
" *\n" \
" * Define SMAA_DISABLE_CORNER_DETECTION to disable corner processing.\n" \
" */\n" \
"#ifndef SMAA_CORNER_ROUNDING\n" \
"#define SMAA_CORNER_ROUNDING 25\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * If there is an neighbor edge that has SMAA_LOCAL_CONTRAST_FACTOR times\n" \
" * bigger contrast than current edge, current edge will be discarded.\n" \
" *\n" \
" * This allows to eliminate spurious crossing edges, and is based on the fact\n" \
" * that, if there is too much contrast in a direction, that will hide\n" \
" * perceptually contrast in the other neighbors.\n" \
" */\n" \
"#ifndef SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR\n" \
"#define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR 2.0\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * Predicated thresholding allows to better preserve texture details and to\n" \
" * improve performance, by decreasing the number of detected edges using an\n" \
" * additional buffer like the light accumulation buffer, object ids or even the\n" \
" * depth buffer (the depth buffer usage may be limited to indoor or short range\n" \
" * scenes).\n" \
" *\n" \
" * It locally decreases the luma or color threshold if an edge is found in an\n" \
" * additional buffer (so the global threshold can be higher).\n" \
" *\n" \
" * This method was developed by Playstation EDGE MLAA team, and used in \n" \
" * Killzone 3, by using the light accumulation buffer. More information here:\n" \
" *     http://iryoku.com/aacourse/downloads/06-MLAA-on-PS3.pptx \n" \
" */\n" \
"#ifndef SMAA_PREDICATION\n" \
"#define SMAA_PREDICATION 0\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * Threshold to be used in the additional predication buffer. \n" \
" *\n" \
" * Range: depends on the input, so you'll have to find the magic number that\n" \
" * works for you.\n" \
" */\n" \
"#ifndef SMAA_PREDICATION_THRESHOLD\n" \
"#define SMAA_PREDICATION_THRESHOLD 0.01\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * How much to scale the global threshold used for luma or color edge\n" \
" * detection when using predication.\n" \
" *\n" \
" * Range: [1, 5]\n" \
" */\n" \
"#ifndef SMAA_PREDICATION_SCALE\n" \
"#define SMAA_PREDICATION_SCALE 2.0\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * How much to locally decrease the threshold.\n" \
" *\n" \
" * Range: [0, 1]\n" \
" */\n" \
"#ifndef SMAA_PREDICATION_STRENGTH\n" \
"#define SMAA_PREDICATION_STRENGTH 0.4\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * Temporal reprojection allows to remove ghosting artifacts when using\n" \
" * temporal supersampling. We use the CryEngine 3 method which also introduces\n" \
" * velocity weighting. This feature is of extreme importance for totally\n" \
" * removing ghosting. More information here:\n" \
" *    http://iryoku.com/aacourse/downloads/13-Anti-Aliasing-Methods-in-CryENGINE-3.pdf\n" \
" *\n" \
" * Note that you'll need to setup a velocity buffer for enabling reprojection.\n" \
" * For static geometry, saving the previous depth buffer is a viable\n" \
" * alternative.\n" \
" */\n" \
"#ifndef SMAA_REPROJECTION\n" \
"#define SMAA_REPROJECTION 0\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * SMAA_REPROJECTION_WEIGHT_SCALE controls the velocity weighting. It allows to\n" \
" * remove ghosting trails behind the moving object, which are not removed by\n" \
" * just using reprojection. Using low values will exhibit ghosting, while using\n" \
" * high values will disable temporal supersampling under motion.\n" \
" *\n" \
" * Behind the scenes, velocity weighting removes temporal supersampling when\n" \
" * the velocity of the subsamples differs (meaning they are different objects).\n" \
" *\n" \
" * Range: [0, 80]\n" \
" */\n" \
"#ifndef SMAA_REPROJECTION_WEIGHT_SCALE\n" \
"#define SMAA_REPROJECTION_WEIGHT_SCALE 30.0\n" \
"#endif\n" \
"\n" \
"/**\n" \
" * On some compilers, discard cannot be used in vertex shaders. Thus, they need\n" \
" * to be compiled separately.\n" \
" */\n" \
"#ifndef SMAA_INCLUDE_VS\n" \
"#define SMAA_INCLUDE_VS 1\n" \
"#endif\n" \
"#ifndef SMAA_INCLUDE_PS\n" \
"#define SMAA_INCLUDE_PS 1\n" \
"#endif\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Texture Access Defines\n" \
"\n" \
"#ifndef SMAA_AREATEX_SELECT\n" \
"#if defined(SMAA_HLSL_3)\n" \
"#define SMAA_AREATEX_SELECT(sample) sample.ra\n" \
"#else\n" \
"#define SMAA_AREATEX_SELECT(sample) sample.rg\n" \
"#endif\n" \
"#endif\n" \
"\n" \
"#ifndef SMAA_SEARCHTEX_SELECT\n" \
"#define SMAA_SEARCHTEX_SELECT(sample) sample.r\n" \
"#endif\n" \
"\n" \
"#ifndef SMAA_DECODE_VELOCITY\n" \
"#define SMAA_DECODE_VELOCITY(sample) sample.rg\n" \
"#endif\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Non-Configurable Defines\n" \
"\n" \
"#define SMAA_AREATEX_MAX_DISTANCE 16\n" \
"#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20\n" \
"#define SMAA_AREATEX_PIXEL_SIZE (1.0 / float2(160.0, 560.0))\n" \
"#define SMAA_AREATEX_SUBTEX_SIZE (1.0 / 7.0)\n" \
"#define SMAA_SEARCHTEX_SIZE float2(66.0, 33.0)\n" \
"#define SMAA_SEARCHTEX_PACKED_SIZE float2(64.0, 16.0)\n" \
"#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0)\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Porting Functions\n" \
"\n" \
"#if defined(SMAA_HLSL_3)\n" \
"#define SMAATexture2D(tex) sampler2D tex\n" \
"#define SMAATexturePass2D(tex) tex\n" \
"#define SMAASampleLevelZero(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))\n" \
"#define SMAASampleLevelZeroPoint(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))\n" \
"#define SMAASampleLevelZeroOffset(tex, coord, offset) tex2Dlod(tex, float4(coord + offset * SMAA_RT_METRICS.xy, 0.0, 0.0))\n" \
"#define SMAASample(tex, coord) tex2D(tex, coord)\n" \
"#define SMAASamplePoint(tex, coord) tex2D(tex, coord)\n" \
"#define SMAASampleOffset(tex, coord, offset) tex2D(tex, coord + offset * SMAA_RT_METRICS.xy)\n" \
"#define SMAA_FLATTEN [flatten]\n" \
"#define SMAA_BRANCH [branch]\n" \
"#endif\n" \
"#if defined(SMAA_HLSL_4) || defined(SMAA_HLSL_4_1)\n" \
"SamplerState LinearSampler { Filter = MIN_MAG_LINEAR_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };\n" \
"SamplerState PointSampler { Filter = MIN_MAG_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };\n" \
"#define SMAATexture2D(tex) Texture2D tex\n" \
"#define SMAATexturePass2D(tex) tex\n" \
"#define SMAASampleLevelZero(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)\n" \
"#define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)\n" \
"#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)\n" \
"#define SMAASample(tex, coord) tex.Sample(LinearSampler, coord)\n" \
"#define SMAASamplePoint(tex, coord) tex.Sample(PointSampler, coord)\n" \
"#define SMAASampleOffset(tex, coord, offset) tex.Sample(LinearSampler, coord, offset)\n" \
"#define SMAA_FLATTEN [flatten]\n" \
"#define SMAA_BRANCH [branch]\n" \
"#define SMAATexture2DMS2(tex) Texture2DMS<float4, 2> tex\n" \
"#define SMAALoad(tex, pos, sample) tex.Load(pos, sample)\n" \
"#if defined(SMAA_HLSL_4_1)\n" \
"#define SMAAGather(tex, coord) tex.Gather(LinearSampler, coord, 0)\n" \
"#endif\n" \
"#endif\n" \
"#if defined(SMAA_GLSL_3) || defined(SMAA_GLSL_4)\n" \
"#define SMAATexture2D(tex) sampler2D tex\n" \
"#define SMAATexturePass2D(tex) tex\n" \
"#define SMAASampleLevelZero(tex, coord) textureLod(tex, coord, 0.0)\n" \
"#define SMAASampleLevelZeroPoint(tex, coord) textureLod(tex, coord, 0.0)\n" \
"#define SMAASampleLevelZeroOffset(tex, coord, offset) textureLodOffset(tex, coord, 0.0, offset)\n" \
"#define SMAASample(tex, coord) texture(tex, coord)\n" \
"#define SMAASamplePoint(tex, coord) texture(tex, coord)\n" \
"#define SMAASampleOffset(tex, coord, offset) texture(tex, coord, offset)\n" \
"#define SMAA_FLATTEN\n" \
"#define SMAA_BRANCH\n" \
"#define lerp(a, b, t) mix(a, b, t)\n" \
"#define saturate(a) clamp(a, 0.0, 1.0)\n" \
"#if defined(SMAA_GLSL_4)\n" \
"#define mad(a, b, c) fma(a, b, c)\n" \
"#define SMAAGather(tex, coord) textureGather(tex, coord)\n" \
"#else\n" \
"#define mad(a, b, c) (a * b + c)\n" \
"#endif\n" \
"#define float2 vec2\n" \
"#define float3 vec3\n" \
"#define float4 vec4\n" \
"#define int2 ivec2\n" \
"#define int3 ivec3\n" \
"#define int4 ivec4\n" \
"#define bool2 bvec2\n" \
"#define bool3 bvec3\n" \
"#define bool4 bvec4\n" \
"#endif\n" \
"\n" \
"#if !defined(SMAA_HLSL_3) && !defined(SMAA_HLSL_4) && !defined(SMAA_HLSL_4_1) && !defined(SMAA_GLSL_3) && !defined(SMAA_GLSL_4) && !defined(SMAA_CUSTOM_SL)\n" \
"#error you must define the shading language: SMAA_HLSL_*, SMAA_GLSL_* or SMAA_CUSTOM_SL\n" \
"#endif\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Misc functions\n" \
"\n" \
"/**\n" \
" * Gathers current pixel, and the top-left neighbors.\n" \
" */\n" \
"float3 SMAAGatherNeighbours(float2 texcoord,\n" \
"                            float4 offset[3],\n" \
"                            SMAATexture2D(tex)) {\n" \
"    #ifdef SMAAGather\n" \
"    return SMAAGather(tex, texcoord + SMAA_RT_METRICS.xy * float2(-0.5, -0.5)).grb;\n" \
"    #else\n" \
"    float P = SMAASamplePoint(tex, texcoord).r;\n" \
"    float Pleft = SMAASamplePoint(tex, offset[0].xy).r;\n" \
"    float Ptop  = SMAASamplePoint(tex, offset[0].zw).r;\n" \
"    return float3(P, Pleft, Ptop);\n" \
"    #endif\n" \
"}\n" \
"\n" \
"/**\n" \
" * Adjusts the threshold by means of predication.\n" \
" */\n" \
"float2 SMAACalculatePredicatedThreshold(float2 texcoord,\n" \
"                                        float4 offset[3],\n" \
"                                        SMAATexture2D(predicationTex)) {\n" \
"    float3 neighbours = SMAAGatherNeighbours(texcoord, offset, SMAATexturePass2D(predicationTex));\n" \
"    float2 delta = abs(neighbours.xx - neighbours.yz);\n" \
"    float2 edges = step(SMAA_PREDICATION_THRESHOLD, delta);\n" \
"    return SMAA_PREDICATION_SCALE * SMAA_THRESHOLD * (1.0 - SMAA_PREDICATION_STRENGTH * edges);\n" \
"}\n" \
"\n" \
"/**\n" \
" * Conditional move:\n" \
" */\n" \
"void SMAAMovc(bool2 cond, inout float2 variable, float2 value) {\n" \
"    SMAA_FLATTEN if (cond.x) variable.x = value.x;\n" \
"    SMAA_FLATTEN if (cond.y) variable.y = value.y;\n" \
"}\n" \
"\n" \
"void SMAAMovc(bool4 cond, inout float4 variable, float4 value) {\n" \
"    SMAAMovc(cond.xy, variable.xy, value.xy);\n" \
"    SMAAMovc(cond.zw, variable.zw, value.zw);\n" \
"}\n" \
"\n" \
"\n" \
"#if SMAA_INCLUDE_VS\n" \
"//-----------------------------------------------------------------------------\n" \
"// Vertex Shaders\n" \
"\n" \
"/**\n" \
" * Edge Detection Vertex Shader\n" \
" */\n" \
"void SMAAEdgeDetectionVS(float2 texcoord,\n" \
"                         out float4 offset[3]) {\n" \
"    offset[0] = mad(SMAA_RT_METRICS.xyxy, float4(-1.0, 0.0, 0.0, -1.0), texcoord.xyxy);\n" \
"    offset[1] = mad(SMAA_RT_METRICS.xyxy, float4( 1.0, 0.0, 0.0,  1.0), texcoord.xyxy);\n" \
"    offset[2] = mad(SMAA_RT_METRICS.xyxy, float4(-2.0, 0.0, 0.0, -2.0), texcoord.xyxy);\n" \
"}\n" \
"\n" \
"/**\n" \
" * Blend Weight Calculation Vertex Shader\n" \
" */\n" \
"void SMAABlendingWeightCalculationVS(float2 texcoord,\n" \
"                                     out float2 pixcoord,\n" \
"                                     out float4 offset[3]) {\n" \
"    pixcoord = texcoord * SMAA_RT_METRICS.zw;\n" \
"\n" \
"    // We will use these offsets for the searches later on (see @PSEUDO_GATHER4):\n" \
"    offset[0] = mad(SMAA_RT_METRICS.xyxy, float4(-0.25, -0.125,  1.25, -0.125), texcoord.xyxy);\n" \
"    offset[1] = mad(SMAA_RT_METRICS.xyxy, float4(-0.125, -0.25, -0.125,  1.25), texcoord.xyxy);\n" \
"\n" \
"    // And these for the searches, they indicate the ends of the loops:\n" \
"    offset[2] = mad(SMAA_RT_METRICS.xxyy,\n" \
"                    float4(-2.0, 2.0, -2.0, 2.0) * float(SMAA_MAX_SEARCH_STEPS),\n" \
"                    float4(offset[0].xz, offset[1].yw));\n" \
"}\n" \
"\n" \
"/**\n" \
" * Neighborhood Blending Vertex Shader\n" \
" */\n" \
"void SMAANeighborhoodBlendingVS(float2 texcoord,\n" \
"                                out float4 offset) {\n" \
"    offset = mad(SMAA_RT_METRICS.xyxy, float4( 1.0, 0.0, 0.0,  1.0), texcoord.xyxy);\n" \
"}\n" \
"#endif // SMAA_INCLUDE_VS\n" \
"\n" \
"#if SMAA_INCLUDE_PS\n" \
"//-----------------------------------------------------------------------------\n" \
"// Edge Detection Pixel Shaders (First Pass)\n" \
"\n" \
"/**\n" \
" * Luma Edge Detection\n" \
" *\n" \
" * IMPORTANT NOTICE: luma edge detection requires gamma-corrected colors, and\n" \
" * thus 'colorTex' should be a non-sRGB texture.\n" \
" */\n" \
"float2 SMAALumaEdgeDetectionPS(float2 texcoord,\n" \
"                               float4 offset[3],\n" \
"                               SMAATexture2D(colorTex)\n" \
"                               #if SMAA_PREDICATION\n" \
"                               , SMAATexture2D(predicationTex)\n" \
"                               #endif\n" \
"                               ) {\n" \
"    // Calculate the threshold:\n" \
"    #if SMAA_PREDICATION\n" \
"    float2 threshold = SMAACalculatePredicatedThreshold(texcoord, offset, SMAATexturePass2D(predicationTex));\n" \
"    #else\n" \
"    float2 threshold = float2(SMAA_THRESHOLD, SMAA_THRESHOLD);\n" \
"    #endif\n" \
"\n" \
"    // Calculate lumas:\n" \
"    float3 weights = float3(0.2126, 0.7152, 0.0722);\n" \
"    float L = dot(SMAASamplePoint(colorTex, texcoord).rgb, weights);\n" \
"\n" \
"    float Lleft = dot(SMAASamplePoint(colorTex, offset[0].xy).rgb, weights);\n" \
"    float Ltop  = dot(SMAASamplePoint(colorTex, offset[0].zw).rgb, weights);\n" \
"\n" \
"    // We do the usual threshold:\n" \
"    float4 delta;\n" \
"    delta.xy = abs(L - float2(Lleft, Ltop));\n" \
"    float2 edges = step(threshold, delta.xy);\n" \
"\n" \
"    // Then discard if there is no edge:\n" \
"    if (dot(edges, float2(1.0, 1.0)) == 0.0)\n" \
"        discard;\n" \
"\n" \
"    // Calculate right and bottom deltas:\n" \
"    float Lright = dot(SMAASamplePoint(colorTex, offset[1].xy).rgb, weights);\n" \
"    float Lbottom  = dot(SMAASamplePoint(colorTex, offset[1].zw).rgb, weights);\n" \
"    delta.zw = abs(L - float2(Lright, Lbottom));\n" \
"\n" \
"    // Calculate the maximum delta in the direct neighborhood:\n" \
"    float2 maxDelta = max(delta.xy, delta.zw);\n" \
"\n" \
"    // Calculate left-left and top-top deltas:\n" \
"    float Lleftleft = dot(SMAASamplePoint(colorTex, offset[2].xy).rgb, weights);\n" \
"    float Ltoptop = dot(SMAASamplePoint(colorTex, offset[2].zw).rgb, weights);\n" \
"    delta.zw = abs(float2(Lleft, Ltop) - float2(Lleftleft, Ltoptop));\n" \
"\n" \
"    // Calculate the final maximum delta:\n" \
"    maxDelta = max(maxDelta.xy, delta.zw);\n" \
"    float finalDelta = max(maxDelta.x, maxDelta.y);\n" \
"\n" \
"    // Local contrast adaptation:\n" \
"    edges.xy *= step(finalDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);\n" \
"\n" \
"    return edges;\n" \
"}\n" \
"\n" \
"/**\n" \
" * Color Edge Detection\n" \
" *\n" \
" * IMPORTANT NOTICE: color edge detection requires gamma-corrected colors, and\n" \
" * thus 'colorTex' should be a non-sRGB texture.\n" \
" */\n" \
"float2 SMAAColorEdgeDetectionPS(float2 texcoord,\n" \
"                                float4 offset[3],\n" \
"                                SMAATexture2D(colorTex)\n" \
"                                #if SMAA_PREDICATION\n" \
"                                , SMAATexture2D(predicationTex)\n" \
"                                #endif\n" \
"                                ) {\n" \
"    // Calculate the threshold:\n" \
"    #if SMAA_PREDICATION\n" \
"    float2 threshold = SMAACalculatePredicatedThreshold(texcoord, offset, predicationTex);\n" \
"    #else\n" \
"    float2 threshold = float2(SMAA_THRESHOLD, SMAA_THRESHOLD);\n" \
"    #endif\n" \
"\n" \
"    // Calculate color deltas:\n" \
"    float4 delta;\n" \
"    float3 C = SMAASamplePoint(colorTex, texcoord).rgb;\n" \
"\n" \
"    float3 Cleft = SMAASamplePoint(colorTex, offset[0].xy).rgb;\n" \
"    float3 t = abs(C - Cleft);\n" \
"    delta.x = max(max(t.r, t.g), t.b);\n" \
"\n" \
"    float3 Ctop  = SMAASamplePoint(colorTex, offset[0].zw).rgb;\n" \
"    t = abs(C - Ctop);\n" \
"    delta.y = max(max(t.r, t.g), t.b);\n" \
"\n" \
"    // We do the usual threshold:\n" \
"    float2 edges = step(threshold, delta.xy);\n" \
"\n" \
"    // Then discard if there is no edge:\n" \
"    if (dot(edges, float2(1.0, 1.0)) == 0.0)\n" \
"        discard;\n" \
"\n" \
"    // Calculate right and bottom deltas:\n" \
"    float3 Cright = SMAASamplePoint(colorTex, offset[1].xy).rgb;\n" \
"    t = abs(C - Cright);\n" \
"    delta.z = max(max(t.r, t.g), t.b);\n" \
"\n" \
"    float3 Cbottom  = SMAASamplePoint(colorTex, offset[1].zw).rgb;\n" \
"    t = abs(C - Cbottom);\n" \
"    delta.w = max(max(t.r, t.g), t.b);\n" \
"\n" \
"    // Calculate the maximum delta in the direct neighborhood:\n" \
"    float2 maxDelta = max(delta.xy, delta.zw);\n" \
"\n" \
"    // Calculate left-left and top-top deltas:\n" \
"    float3 Cleftleft  = SMAASamplePoint(colorTex, offset[2].xy).rgb;\n" \
"    t = abs(Cleft - Cleftleft);\n" \
"    delta.z = max(max(t.r, t.g), t.b);\n" \
"\n" \
"    float3 Ctoptop = SMAASamplePoint(colorTex, offset[2].zw).rgb;\n" \
"    t = abs(Ctop - Ctoptop);\n" \
"    delta.w = max(max(t.r, t.g), t.b);\n" \
"\n" \
"    // Calculate the final maximum delta:\n" \
"    maxDelta = max(maxDelta.xy, delta.zw);\n" \
"    float finalDelta = max(maxDelta.x, maxDelta.y);\n" \
"\n" \
"    // Local contrast adaptation:\n" \
"    edges.xy *= step(finalDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);\n" \
"\n" \
"    return edges;\n" \
"}\n" \
"\n" \
"/**\n" \
" * Depth Edge Detection\n" \
" */\n" \
"float2 SMAADepthEdgeDetectionPS(float2 texcoord,\n" \
"                                float4 offset[3],\n" \
"                                SMAATexture2D(depthTex)) {\n" \
"    float3 neighbours = SMAAGatherNeighbours(texcoord, offset, SMAATexturePass2D(depthTex));\n" \
"    float2 delta = abs(neighbours.xx - float2(neighbours.y, neighbours.z));\n" \
"    float2 edges = step(SMAA_DEPTH_THRESHOLD, delta);\n" \
"\n" \
"    if (dot(edges, float2(1.0, 1.0)) == 0.0)\n" \
"        discard;\n" \
"\n" \
"    return edges;\n" \
"}\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Diagonal Search Functions\n" \
"\n" \
"#if !defined(SMAA_DISABLE_DIAG_DETECTION)\n" \
"\n" \
"/**\n" \
" * Allows to decode two binary values from a bilinear-filtered access.\n" \
" */\n" \
"float2 SMAADecodeDiagBilinearAccess(float2 e) {\n" \
"    // Bilinear access for fetching 'e' have a 0.25 offset, and we are\n" \
"    // interested in the R and G edges:\n" \
"    //\n" \
"    // +---G---+-------+\n" \
"    // |   x o R   x   |\n" \
"    // +-------+-------+\n" \
"    //\n" \
"    // Then, if one of these edge is enabled:\n" \
"    //   Red:   (0.75 * X + 0.25 * 1) => 0.25 or 1.0\n" \
"    //   Green: (0.75 * 1 + 0.25 * X) => 0.75 or 1.0\n" \
"    //\n" \
"    // This function will unpack the values (mad + mul + round):\n" \
"    // wolframalpha.com: round(x * abs(5 * x - 5 * 0.75)) plot 0 to 1\n" \
"    e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);\n" \
"    return round(e);\n" \
"}\n" \
"\n" \
"float4 SMAADecodeDiagBilinearAccess(float4 e) {\n" \
"    e.rb = e.rb * abs(5.0 * e.rb - 5.0 * 0.75);\n" \
"    return round(e);\n" \
"}\n" \
"\n" \
"/**\n" \
" * These functions allows to perform diagonal pattern searches.\n" \
" */\n" \
"float2 SMAASearchDiag1(SMAATexture2D(edgesTex), float2 texcoord, float2 dir, out float2 e) {\n" \
"    float4 coord = float4(texcoord, -1.0, 1.0);\n" \
"    float3 t = float3(SMAA_RT_METRICS.xy, 1.0);\n" \
"    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) &&\n" \
"           coord.w > 0.9) {\n" \
"        coord.xyz = mad(t, float3(dir, 1.0), coord.xyz);\n" \
"        e = SMAASampleLevelZero(edgesTex, coord.xy).rg;\n" \
"        coord.w = dot(e, float2(0.5, 0.5));\n" \
"    }\n" \
"    return coord.zw;\n" \
"}\n" \
"\n" \
"float2 SMAASearchDiag2(SMAATexture2D(edgesTex), float2 texcoord, float2 dir, out float2 e) {\n" \
"    float4 coord = float4(texcoord, -1.0, 1.0);\n" \
"    coord.x += 0.25 * SMAA_RT_METRICS.x; // See @SearchDiag2Optimization\n" \
"    float3 t = float3(SMAA_RT_METRICS.xy, 1.0);\n" \
"    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) &&\n" \
"           coord.w > 0.9) {\n" \
"        coord.xyz = mad(t, float3(dir, 1.0), coord.xyz);\n" \
"\n" \
"        // @SearchDiag2Optimization\n" \
"        // Fetch both edges at once using bilinear filtering:\n" \
"        e = SMAASampleLevelZero(edgesTex, coord.xy).rg;\n" \
"        e = SMAADecodeDiagBilinearAccess(e);\n" \
"\n" \
"        // Non-optimized version:\n" \
"        // e.g = SMAASampleLevelZero(edgesTex, coord.xy).g;\n" \
"        // e.r = SMAASampleLevelZeroOffset(edgesTex, coord.xy, int2(1, 0)).r;\n" \
"\n" \
"        coord.w = dot(e, float2(0.5, 0.5));\n" \
"    }\n" \
"    return coord.zw;\n" \
"}\n" \
"\n" \
"/** \n" \
" * Similar to SMAAArea, this calculates the area corresponding to a certain\n" \
" * diagonal distance and crossing edges 'e'.\n" \
" */\n" \
"float2 SMAAAreaDiag(SMAATexture2D(areaTex), float2 dist, float2 e, float offset) {\n" \
"    float2 texcoord = mad(float2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG), e, dist);\n" \
"\n" \
"    // We do a scale and bias for mapping to texel space:\n" \
"    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);\n" \
"\n" \
"    // Diagonal areas are on the second half of the texture:\n" \
"    texcoord.x += 0.5;\n" \
"\n" \
"    // Move to proper place, according to the subpixel offset:\n" \
"    texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;\n" \
"\n" \
"    // Do it!\n" \
"    return SMAA_AREATEX_SELECT(SMAASampleLevelZero(areaTex, texcoord));\n" \
"}\n" \
"\n" \
"/**\n" \
" * This searches for diagonal patterns and returns the corresponding weights.\n" \
" */\n" \
"float2 SMAACalculateDiagWeights(SMAATexture2D(edgesTex), SMAATexture2D(areaTex), float2 texcoord, float2 e, float4 subsampleIndices) {\n" \
"    float2 weights = float2(0.0, 0.0);\n" \
"\n" \
"    // Search for the line ends:\n" \
"    float4 d;\n" \
"    float2 end;\n" \
"    if (e.r > 0.0) {\n" \
"        d.xz = SMAASearchDiag1(SMAATexturePass2D(edgesTex), texcoord, float2(-1.0,  1.0), end);\n" \
"        d.x += float(end.y > 0.9);\n" \
"    } else\n" \
"        d.xz = float2(0.0, 0.0);\n" \
"    d.yw = SMAASearchDiag1(SMAATexturePass2D(edgesTex), texcoord, float2(1.0, -1.0), end);\n" \
"\n" \
"    SMAA_BRANCH\n" \
"    if (d.x + d.y > 2.0) { // d.x + d.y + 1 > 3\n" \
"        // Fetch the crossing edges:\n" \
"        float4 coords = mad(float4(-d.x + 0.25, d.x, d.y, -d.y - 0.25), SMAA_RT_METRICS.xyxy, texcoord.xyxy);\n" \
"        float4 c;\n" \
"        c.xy = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(-1,  0)).rg;\n" \
"        c.zw = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1,  0)).rg;\n" \
"        c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);\n" \
"\n" \
"        // Non-optimized version:\n" \
"        // float4 coords = mad(float4(-d.x, d.x, d.y, -d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);\n" \
"        // float4 c;\n" \
"        // c.x = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(-1,  0)).g;\n" \
"        // c.y = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2( 0,  0)).r;\n" \
"        // c.z = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1,  0)).g;\n" \
"        // c.w = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1, -1)).r;\n" \
"\n" \
"        // Merge crossing edges at each side into a single value:\n" \
"        float2 cc = mad(float2(2.0, 2.0), c.xz, c.yw);\n" \
"\n" \
"        // Remove the crossing edge if we didn't found the end of the line:\n" \
"        SMAAMovc(bool2(step(0.9, d.zw)), cc, float2(0.0, 0.0));\n" \
"\n" \
"        // Fetch the areas for this line:\n" \
"        weights += SMAAAreaDiag(SMAATexturePass2D(areaTex), d.xy, cc, subsampleIndices.z);\n" \
"    }\n" \
"\n" \
"    // Search for the line ends:\n" \
"    d.xz = SMAASearchDiag2(SMAATexturePass2D(edgesTex), texcoord, float2(-1.0, -1.0), end);\n" \
"    if (SMAASampleLevelZeroOffset(edgesTex, texcoord, int2(1, 0)).r > 0.0) {\n" \
"        d.yw = SMAASearchDiag2(SMAATexturePass2D(edgesTex), texcoord, float2(1.0, 1.0), end);\n" \
"        d.y += float(end.y > 0.9);\n" \
"    } else\n" \
"        d.yw = float2(0.0, 0.0);\n" \
"\n" \
"    SMAA_BRANCH\n" \
"    if (d.x + d.y > 2.0) { // d.x + d.y + 1 > 3\n" \
"        // Fetch the crossing edges:\n" \
"        float4 coords = mad(float4(-d.x, -d.x, d.y, d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);\n" \
"        float4 c;\n" \
"        c.x  = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(-1,  0)).g;\n" \
"        c.y  = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2( 0, -1)).r;\n" \
"        c.zw = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1,  0)).gr;\n" \
"        float2 cc = mad(float2(2.0, 2.0), c.xz, c.yw);\n" \
"\n" \
"        // Remove the crossing edge if we didn't found the end of the line:\n" \
"        SMAAMovc(bool2(step(0.9, d.zw)), cc, float2(0.0, 0.0));\n" \
"\n" \
"        // Fetch the areas for this line:\n" \
"        weights += SMAAAreaDiag(SMAATexturePass2D(areaTex), d.xy, cc, subsampleIndices.w).gr;\n" \
"    }\n" \
"\n" \
"    return weights;\n" \
"}\n" \
"#endif\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Horizontal/Vertical Search Functions\n" \
"\n" \
"/**\n" \
" * This allows to determine how much length should we add in the last step\n" \
" * of the searches. It takes the bilinearly interpolated edge (see \n" \
" * @PSEUDO_GATHER4), and adds 0, 1 or 2, depending on which edges and\n" \
" * crossing edges are active.\n" \
" */\n" \
"float SMAASearchLength(SMAATexture2D(searchTex), float2 e, float offset) {\n" \
"    // The texture is flipped vertically, with left and right cases taking half\n" \
"    // of the space horizontally:\n" \
"    float2 scale = SMAA_SEARCHTEX_SIZE * float2(0.5, -1.0);\n" \
"    float2 bias = SMAA_SEARCHTEX_SIZE * float2(offset, 1.0);\n" \
"\n" \
"    // Scale and bias to access texel centers:\n" \
"    scale += float2(-1.0,  1.0);\n" \
"    bias  += float2( 0.5, -0.5);\n" \
"\n" \
"    // Convert from pixel coordinates to texcoords:\n" \
"    // (We use SMAA_SEARCHTEX_PACKED_SIZE because the texture is cropped)\n" \
"    scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;\n" \
"    bias *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;\n" \
"\n" \
"    // Lookup the search texture:\n" \
"    return SMAA_SEARCHTEX_SELECT(SMAASampleLevelZero(searchTex, mad(scale, e, bias)));\n" \
"}\n" \
"\n" \
"/**\n" \
" * Horizontal/vertical search functions for the 2nd pass.\n" \
" */\n" \
"float SMAASearchXLeft(SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end) {\n" \
"    /**\n" \
"     * @PSEUDO_GATHER4\n" \
"     * This texcoord has been offset by (-0.25, -0.125) in the vertex shader to\n" \
"     * sample between edge, thus fetching four edges in a row.\n" \
"     * Sampling with different offsets in each direction allows to disambiguate\n" \
"     * which edges are active from the four fetched ones.\n" \
"     */\n" \
"    float2 e = float2(0.0, 1.0);\n" \
"    while (texcoord.x > end && \n" \
"           e.g > 0.8281 && // Is there some edge not activated?\n" \
"           e.r == 0.0) { // Or is there a crossing edge that breaks the line?\n" \
"        e = SMAASampleLevelZero(edgesTex, texcoord).rg;\n" \
"        texcoord = mad(-float2(2.0, 0.0), SMAA_RT_METRICS.xy, texcoord);\n" \
"    }\n" \
"\n" \
"    float offset = mad(-(255.0 / 127.0), SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.0), 3.25);\n" \
"    return mad(SMAA_RT_METRICS.x, offset, texcoord.x);\n" \
"\n" \
"    // Non-optimized version:\n" \
"    // We correct the previous (-0.25, -0.125) offset we applied:\n" \
"    // texcoord.x += 0.25 * SMAA_RT_METRICS.x;\n" \
"\n" \
"    // The searches are bias by 1, so adjust the coords accordingly:\n" \
"    // texcoord.x += SMAA_RT_METRICS.x;\n" \
"\n" \
"    // Disambiguate the length added by the last step:\n" \
"    // texcoord.x += 2.0 * SMAA_RT_METRICS.x; // Undo last step\n" \
"    // texcoord.x -= SMAA_RT_METRICS.x * (255.0 / 127.0) * SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.0);\n" \
"    // return mad(SMAA_RT_METRICS.x, offset, texcoord.x);\n" \
"}\n" \
"\n" \
"float SMAASearchXRight(SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end) {\n" \
"    float2 e = float2(0.0, 1.0);\n" \
"    while (texcoord.x < end && \n" \
"           e.g > 0.8281 && // Is there some edge not activated?\n" \
"           e.r == 0.0) { // Or is there a crossing edge that breaks the line?\n" \
"        e = SMAASampleLevelZero(edgesTex, texcoord).rg;\n" \
"        texcoord = mad(float2(2.0, 0.0), SMAA_RT_METRICS.xy, texcoord);\n" \
"    }\n" \
"    float offset = mad(-(255.0 / 127.0), SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.5), 3.25);\n" \
"    return mad(-SMAA_RT_METRICS.x, offset, texcoord.x);\n" \
"}\n" \
"\n" \
"float SMAASearchYUp(SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end) {\n" \
"    float2 e = float2(1.0, 0.0);\n" \
"    while (texcoord.y > end && \n" \
"           e.r > 0.8281 && // Is there some edge not activated?\n" \
"           e.g == 0.0) { // Or is there a crossing edge that breaks the line?\n" \
"        e = SMAASampleLevelZero(edgesTex, texcoord).rg;\n" \
"        texcoord = mad(-float2(0.0, 2.0), SMAA_RT_METRICS.xy, texcoord);\n" \
"    }\n" \
"    float offset = mad(-(255.0 / 127.0), SMAASearchLength(SMAATexturePass2D(searchTex), e.gr, 0.0), 3.25);\n" \
"    return mad(SMAA_RT_METRICS.y, offset, texcoord.y);\n" \
"}\n" \
"\n" \
"float SMAASearchYDown(SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end) {\n" \
"    float2 e = float2(1.0, 0.0);\n" \
"    while (texcoord.y < end && \n" \
"           e.r > 0.8281 && // Is there some edge not activated?\n" \
"           e.g == 0.0) { // Or is there a crossing edge that breaks the line?\n" \
"        e = SMAASampleLevelZero(edgesTex, texcoord).rg;\n" \
"        texcoord = mad(float2(0.0, 2.0), SMAA_RT_METRICS.xy, texcoord);\n" \
"    }\n" \
"    float offset = mad(-(255.0 / 127.0), SMAASearchLength(SMAATexturePass2D(searchTex), e.gr, 0.5), 3.25);\n" \
"    return mad(-SMAA_RT_METRICS.y, offset, texcoord.y);\n" \
"}\n" \
"\n" \
"/** \n" \
" * Ok, we have the distance and both crossing edges. So, what are the areas\n" \
" * at each side of current edge?\n" \
" */\n" \
"float2 SMAAArea(SMAATexture2D(areaTex), float2 dist, float e1, float e2, float offset) {\n" \
"    // Rounding prevents precision errors of bilinear filtering:\n" \
"    float2 texcoord = mad(float2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE), round(4.0 * float2(e1, e2)), dist);\n" \
"    \n" \
"    // We do a scale and bias for mapping to texel space:\n" \
"    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);\n" \
"\n" \
"    // Move to proper place, according to the subpixel offset:\n" \
"    texcoord.y = mad(SMAA_AREATEX_SUBTEX_SIZE, offset, texcoord.y);\n" \
"\n" \
"    // Do it!\n" \
"    return SMAA_AREATEX_SELECT(SMAASampleLevelZero(areaTex, texcoord));\n" \
"}\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Corner Detection Functions\n" \
"\n" \
"void SMAADetectHorizontalCornerPattern(SMAATexture2D(edgesTex), inout float2 weights, float4 texcoord, float2 d) {\n" \
"    #if !defined(SMAA_DISABLE_CORNER_DETECTION)\n" \
"    float2 leftRight = step(d.xy, d.yx);\n" \
"    float2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;\n" \
"\n" \
"    rounding /= leftRight.x + leftRight.y; // Reduce blending for pixels in the center of a line.\n" \
"\n" \
"    float2 factor = float2(1.0, 1.0);\n" \
"    factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2(0,  1)).r;\n" \
"    factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2(1,  1)).r;\n" \
"    factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2(0, -2)).r;\n" \
"    factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2(1, -2)).r;\n" \
"\n" \
"    weights *= saturate(factor);\n" \
"    #endif\n" \
"}\n" \
"\n" \
"void SMAADetectVerticalCornerPattern(SMAATexture2D(edgesTex), inout float2 weights, float4 texcoord, float2 d) {\n" \
"    #if !defined(SMAA_DISABLE_CORNER_DETECTION)\n" \
"    float2 leftRight = step(d.xy, d.yx);\n" \
"    float2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;\n" \
"\n" \
"    rounding /= leftRight.x + leftRight.y;\n" \
"\n" \
"    float2 factor = float2(1.0, 1.0);\n" \
"    factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2( 1, 0)).g;\n" \
"    factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2( 1, 1)).g;\n" \
"    factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2(-2, 0)).g;\n" \
"    factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2(-2, 1)).g;\n" \
"\n" \
"    weights *= saturate(factor);\n" \
"    #endif\n" \
"}\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Blending Weight Calculation Pixel Shader (Second Pass)\n" \
"\n" \
"float4 SMAABlendingWeightCalculationPS(float2 texcoord,\n" \
"                                       float2 pixcoord,\n" \
"                                       float4 offset[3],\n" \
"                                       SMAATexture2D(edgesTex),\n" \
"                                       SMAATexture2D(areaTex),\n" \
"                                       SMAATexture2D(searchTex),\n" \
"                                       float4 subsampleIndices) { // Just pass zero for SMAA 1x, see @SUBSAMPLE_INDICES.\n" \
"    float4 weights = float4(0.0, 0.0, 0.0, 0.0);\n" \
"\n" \
"    float2 e = SMAASample(edgesTex, texcoord).rg;\n" \
"\n" \
"    SMAA_BRANCH\n" \
"    if (e.g > 0.0) { // Edge at north\n" \
"        #if !defined(SMAA_DISABLE_DIAG_DETECTION)\n" \
"        // Diagonals have both north and west edges, so searching for them in\n" \
"        // one of the boundaries is enough.\n" \
"        weights.rg = SMAACalculateDiagWeights(SMAATexturePass2D(edgesTex), SMAATexturePass2D(areaTex), texcoord, e, subsampleIndices);\n" \
"\n" \
"        // We give priority to diagonals, so if we find a diagonal we skip \n" \
"        // horizontal/vertical processing.\n" \
"        SMAA_BRANCH\n" \
"        if (weights.r == -weights.g) { // weights.r + weights.g == 0.0\n" \
"        #endif\n" \
"\n" \
"        float2 d;\n" \
"\n" \
"        // Find the distance to the left:\n" \
"        float3 coords;\n" \
"        coords.x = SMAASearchXLeft(SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[0].xy, offset[2].x);\n" \
"        coords.y = offset[1].y; // offset[1].y = texcoord.y - 0.25 * SMAA_RT_METRICS.y (@CROSSING_OFFSET)\n" \
"        d.x = coords.x;\n" \
"\n" \
"        // Now fetch the left crossing edges, two at a time using bilinear\n" \
"        // filtering. Sampling at -0.25 (see @CROSSING_OFFSET) enables to\n" \
"        // discern what value each edge has:\n" \
"        float e1 = SMAASampleLevelZero(edgesTex, coords.xy).r;\n" \
"\n" \
"        // Find the distance to the right:\n" \
"        coords.z = SMAASearchXRight(SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[0].zw, offset[2].y);\n" \
"        d.y = coords.z;\n" \
"\n" \
"        // We want the distances to be in pixel units (doing this here allow to\n" \
"        // better interleave arithmetic and memory accesses):\n" \
"        d = abs(round(mad(SMAA_RT_METRICS.zz, d, -pixcoord.xx)));\n" \
"\n" \
"        // SMAAArea below needs a sqrt, as the areas texture is compressed\n" \
"        // quadratically:\n" \
"        float2 sqrt_d = sqrt(d);\n" \
"\n" \
"        // Fetch the right crossing edges:\n" \
"        float e2 = SMAASampleLevelZeroOffset(edgesTex, coords.zy, int2(1, 0)).r;\n" \
"\n" \
"        // Ok, we know how this pattern looks like, now it is time for getting\n" \
"        // the actual area:\n" \
"        weights.rg = SMAAArea(SMAATexturePass2D(areaTex), sqrt_d, e1, e2, subsampleIndices.y);\n" \
"\n" \
"        // Fix corners:\n" \
"        coords.y = texcoord.y;\n" \
"        SMAADetectHorizontalCornerPattern(SMAATexturePass2D(edgesTex), weights.rg, coords.xyzy, d);\n" \
"\n" \
"        #if !defined(SMAA_DISABLE_DIAG_DETECTION)\n" \
"        } else\n" \
"            e.r = 0.0; // Skip vertical processing.\n" \
"        #endif\n" \
"    }\n" \
"\n" \
"    SMAA_BRANCH\n" \
"    if (e.r > 0.0) { // Edge at west\n" \
"        float2 d;\n" \
"\n" \
"        // Find the distance to the top:\n" \
"        float3 coords;\n" \
"        coords.y = SMAASearchYUp(SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[1].xy, offset[2].z);\n" \
"        coords.x = offset[0].x; // offset[1].x = texcoord.x - 0.25 * SMAA_RT_METRICS.x;\n" \
"        d.x = coords.y;\n" \
"\n" \
"        // Fetch the top crossing edges:\n" \
"        float e1 = SMAASampleLevelZero(edgesTex, coords.xy).g;\n" \
"\n" \
"        // Find the distance to the bottom:\n" \
"        coords.z = SMAASearchYDown(SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[1].zw, offset[2].w);\n" \
"        d.y = coords.z;\n" \
"\n" \
"        // We want the distances to be in pixel units:\n" \
"        d = abs(round(mad(SMAA_RT_METRICS.ww, d, -pixcoord.yy)));\n" \
"\n" \
"        // SMAAArea below needs a sqrt, as the areas texture is compressed \n" \
"        // quadratically:\n" \
"        float2 sqrt_d = sqrt(d);\n" \
"\n" \
"        // Fetch the bottom crossing edges:\n" \
"        float e2 = SMAASampleLevelZeroOffset(edgesTex, coords.xz, int2(0, 1)).g;\n" \
"\n" \
"        // Get the area for this direction:\n" \
"        weights.ba = SMAAArea(SMAATexturePass2D(areaTex), sqrt_d, e1, e2, subsampleIndices.x);\n" \
"\n" \
"        // Fix corners:\n" \
"        coords.x = texcoord.x;\n" \
"        SMAADetectVerticalCornerPattern(SMAATexturePass2D(edgesTex), weights.ba, coords.xyxz, d);\n" \
"    }\n" \
"\n" \
"    return weights;\n" \
"}\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Neighborhood Blending Pixel Shader (Third Pass)\n" \
"\n" \
"float4 SMAANeighborhoodBlendingPS(float2 texcoord,\n" \
"                                  float4 offset,\n" \
"                                  SMAATexture2D(colorTex),\n" \
"                                  SMAATexture2D(blendTex)\n" \
"                                  #if SMAA_REPROJECTION\n" \
"                                  , SMAATexture2D(velocityTex)\n" \
"                                  #endif\n" \
"                                  ) {\n" \
"    // Fetch the blending weights for current pixel:\n" \
"    float4 a;\n" \
"    a.x = SMAASample(blendTex, offset.xy).a; // Right\n" \
"    a.y = SMAASample(blendTex, offset.zw).g; // Top\n" \
"    a.wz = SMAASample(blendTex, texcoord).xz; // Bottom / Left\n" \
"\n" \
"    // Is there any blending weight with a value greater than 0.0?\n" \
"    SMAA_BRANCH\n" \
"    if (dot(a, float4(1.0, 1.0, 1.0, 1.0)) < 1e-5) {\n" \
"        float4 color = SMAASampleLevelZero(colorTex, texcoord);\n" \
"\n" \
"        #if SMAA_REPROJECTION\n" \
"        float2 velocity = SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, texcoord));\n" \
"\n" \
"        // Pack velocity into the alpha channel:\n" \
"        color.a = sqrt(5.0 * length(velocity));\n" \
"        #endif\n" \
"\n" \
"        return color;\n" \
"    } else {\n" \
"        bool h = max(a.x, a.z) > max(a.y, a.w); // max(horizontal) > max(vertical)\n" \
"\n" \
"        // Calculate the blending offsets:\n" \
"        float4 blendingOffset = float4(0.0, a.y, 0.0, a.w);\n" \
"        float2 blendingWeight = a.yw;\n" \
"        SMAAMovc(bool4(h, h, h, h), blendingOffset, float4(a.x, 0.0, a.z, 0.0));\n" \
"        SMAAMovc(bool2(h, h), blendingWeight, a.xz);\n" \
"        blendingWeight /= dot(blendingWeight, float2(1.0, 1.0));\n" \
"\n" \
"        // Calculate the texture coordinates:\n" \
"        float4 blendingCoord = mad(blendingOffset, float4(SMAA_RT_METRICS.xy, -SMAA_RT_METRICS.xy), texcoord.xyxy);\n" \
"\n" \
"        // We exploit bilinear filtering to mix current pixel with the chosen\n" \
"        // neighbor:\n" \
"        float4 color = blendingWeight.x * SMAASampleLevelZero(colorTex, blendingCoord.xy);\n" \
"        color += blendingWeight.y * SMAASampleLevelZero(colorTex, blendingCoord.zw);\n" \
"\n" \
"        #if SMAA_REPROJECTION\n" \
"        // Antialias velocity for proper reprojection in a later stage:\n" \
"        float2 velocity = blendingWeight.x * SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, blendingCoord.xy));\n" \
"        velocity += blendingWeight.y * SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, blendingCoord.zw));\n" \
"\n" \
"        // Pack velocity into the alpha channel:\n" \
"        color.a = sqrt(5.0 * length(velocity));\n" \
"        #endif\n" \
"\n" \
"        return color;\n" \
"    }\n" \
"}\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Temporal Resolve Pixel Shader (Optional Pass)\n" \
"\n" \
"float4 SMAAResolvePS(float2 texcoord,\n" \
"                     SMAATexture2D(currentColorTex),\n" \
"                     SMAATexture2D(previousColorTex)\n" \
"                     #if SMAA_REPROJECTION\n" \
"                     , SMAATexture2D(velocityTex)\n" \
"                     #endif\n" \
"                     ) {\n" \
"    #if SMAA_REPROJECTION\n" \
"    // Velocity is assumed to be calculated for motion blur, so we need to\n" \
"    // inverse it for reprojection:\n" \
"    float2 velocity = -SMAA_DECODE_VELOCITY(SMAASamplePoint(velocityTex, texcoord).rg);\n" \
"\n" \
"    // Fetch current pixel:\n" \
"    float4 current = SMAASamplePoint(currentColorTex, texcoord);\n" \
"\n" \
"    // Reproject current coordinates and fetch previous pixel:\n" \
"    float4 previous = SMAASamplePoint(previousColorTex, texcoord + velocity);\n" \
"\n" \
"    // Attenuate the previous pixel if the velocity is different:\n" \
"    float delta = abs(current.a * current.a - previous.a * previous.a) / 5.0;\n" \
"    float weight = 0.5 * saturate(1.0 - sqrt(delta) * SMAA_REPROJECTION_WEIGHT_SCALE);\n" \
"\n" \
"    // Blend the pixels according to the calculated weight:\n" \
"    return lerp(current, previous, weight);\n" \
"    #else\n" \
"    // Just blend the pixels:\n" \
"    float4 current = SMAASamplePoint(currentColorTex, texcoord);\n" \
"    float4 previous = SMAASamplePoint(previousColorTex, texcoord);\n" \
"    return lerp(current, previous, 0.5);\n" \
"    #endif\n" \
"}\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"// Separate Multisamples Pixel Shader (Optional Pass)\n" \
"\n" \
"#ifdef SMAALoad\n" \
"void SMAASeparatePS(float4 position,\n" \
"                    float2 texcoord,\n" \
"                    out float4 target0,\n" \
"                    out float4 target1,\n" \
"                    SMAATexture2DMS2(colorTexMS)) {\n" \
"    int2 pos = int2(position.xy);\n" \
"    target0 = SMAALoad(colorTexMS, pos, 0);\n" \
"    target1 = SMAALoad(colorTexMS, pos, 1);\n" \
"}\n" \
"#endif\n" \
"\n" \
"//-----------------------------------------------------------------------------\n" \
"#endif // SMAA_INCLUDE_PS\n" \
""

#endif /* SMAA_HLSL_H */
