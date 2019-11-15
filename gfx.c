#include <hal/video.h>
#include <hal/xbox.h>
#include <math.h>
#include <pbkit/pbkit.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/debug.h>

static float m_viewport[4][4];

#pragma pack(1)
struct ColoredVertex {
    float pos[3];
    float color[3];
};
#pragma pack()

#define MASK(mask, val) (((val) << (ffs(mask)-1)) & (mask))

static void gfx_load_shader(void);
static void gfx_set_attrib_pointer(unsigned int index, unsigned int format, unsigned int size, unsigned int stride, const void* data);
static void matrix_viewport(float out[4][4], float x, float y, float width, float height, float z_min, float z_max);

/* Init graphics */
void gfx_init(void)
{
    int status;
    int width, height;

    if ((status = pb_init())) {
        debugPrint("pb_init Error %d\n", status);
        XSleep(2000);
        XReboot();
        return;
    }

    /* Basic setup */
    width = pb_back_buffer_width();
    height = pb_back_buffer_height();

    /* Load constant rendering things (shaders, geometry) */
    gfx_load_shader();
    matrix_viewport(m_viewport, 0, 0, width, height, 0, 65536.0f);
}

void gfx_begin(void)
{
    int width, height;

    pb_wait_for_vbl();
    pb_reset();
    pb_target_back_buffer();

    /* Basic setup */
    width = pb_back_buffer_width();
    height = pb_back_buffer_height();

    /* Clear depth & stencil buffers */
    pb_erase_depth_stencil_buffer(0, 0, width, height);
    pb_fill(0, 0, width, height, 0x00000000);
    // pb_erase_text_screen();

    while(pb_busy()) {
        /* Wait for completion... */
    }
}

void gfx_end(void)
{
    while(pb_busy()) {
        /* Wait for completion... */
    }
}

void gfx_present(void)
{
    /* Swap buffers (if we can) */
    while (pb_finished()) {
        /* Not ready to swap yet */
    }
}

void gfx_draw_bg(void)
{
    uint32_t *p;
    int i;

    /* Send shader constants */
    p = pb_begin();

    /* Set shader constants cursor at C0 */
    pb_push1(p, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_ID, 96); p+=2;

    /* Send the transformation matrix */
    pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 16);
    memcpy(p, m_viewport, 16*4); p+=16;

    pb_end(p);
    p = pb_begin();

    /* Clear all attributes */
    pb_push(p++,NV097_SET_VERTEX_DATA_ARRAY_FORMAT,16);
    for(i = 0; i < 16; i++) {
        *(p++) = 2;
    }
    pb_end(p);

    /* Set vertex position attribute */
    gfx_set_attrib_pointer(0, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F,
                       3, sizeof(struct ColoredVertex), NULL);
    
    /* Set vertex diffuse color attribute */
    gfx_set_attrib_pointer(3, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F,
                       3, sizeof(struct ColoredVertex), NULL);

    /* Begin drawing triangles */
    struct ColoredVertex quad[] = {
        //  X     Y     Z       R     G     B
        {{-1.0, -1.0,  1.0}, { 0.1,  0.1,  0.6}}, /* Background triangle 1 */
        {{-1.0,  1.0,  1.0}, { 0.0,  0.0,  0.0}},
        {{ 1.0,  1.0,  1.0}, { 0.0,  0.0,  0.0}},
        {{ 1.0, -1.0,  1.0}, { 0.1,  0.1,  0.6}},
    };

    p = pb_begin();
    pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_QUADS); p += 2;
    pb_push(p++,0x40000000|NV097_INLINE_ARRAY,sizeof(quad)/4); //bit 30 means all params go to same register 0x1810
    memcpy(p, quad, sizeof(quad));
    p += sizeof(quad)/4;
    pb_push1(p,NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END); p += 2;
    pb_end(p);
}

/* Construct a viewport transformation matrix */
static void matrix_viewport(float out[4][4], float x, float y, float width, float height, float z_min, float z_max)
{
    memset(out, 0, 4*4*sizeof(float));
    out[0][0] = width/2.0f;
    out[1][1] = height/-2.0f;
    out[2][2] = z_max - z_min;
    out[3][3] = 1.0f;
    out[3][0] = x + width/2.0f;
    out[3][1] = y + height/2.0f;
    out[3][2] = z_min;
}

/* Load the shader we will render with */
static void gfx_load_shader(void)
{
    uint32_t *p;
    int       i;

    /* Setup vertex shader */
    uint32_t vs_program[] = {
        #include "vs.inl"
    };

    p = pb_begin();

    /* Set run address of shader */
    pb_push(p++, NV097_SET_TRANSFORM_PROGRAM_START, 1); *(p++)=0;

    /* Set execution mode */
    pb_push(p++, NV097_SET_TRANSFORM_EXECUTION_MODE, 2);
    *(p++) = SHADER_TYPE_EXTERNAL;
    *(p++) = SHADER_SUBTYPE_REGULAR;

    /* Set cursor and begin copying program */
    pb_push(p++, NV097_SET_TRANSFORM_PROGRAM_LOAD, 1); *(p++)=0;
    for (i=0; i<sizeof(vs_program)/8; i++) {
        pb_push(p++,NV097_SET_TRANSFORM_PROGRAM,4);
        memcpy(p, &vs_program[i*4], 4*4);
        p+=4;
    }
    pb_end(p);

    /* Setup fragment shader */
    p = pb_begin();
    #include "ps.inl"
    pb_end(p);
}

/* Set an attribute pointer */
static void gfx_set_attrib_pointer(unsigned int index, unsigned int format, unsigned int size, unsigned int stride, const void* data)
{
    uint32_t *p = pb_begin();
    pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT + index*4,
        MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE, format) | \
        MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE, size) | \
        MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE, stride));
    p += 2;
    // pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_OFFSET + index*4, (uint32_t)data & 0x03ffffff);
    // p += 2;
    pb_end(p);
}
