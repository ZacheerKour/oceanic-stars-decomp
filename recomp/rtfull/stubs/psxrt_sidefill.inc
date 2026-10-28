/* Side fill drawing, #included into gpu_gl_renderer.c before gl_swap_with_osd (CMakeLists.txt). Runs after the picture is composed
 * and before the host OSD / F10 menu: the picture rect (the same letterbox every present uses) is copied out of the default
 * framebuffer and one window-sized triangle is drawn over everything outside that rect. blur = the copy, cover-scaled, blurred
 * and darkened; art = a local PNG (stubs/psxrt_sidefill.c) cover-scaled and faded into the blur toward the picture edge. */
int psxrt_sidefill_mode(void);
unsigned char *psxrt_sidefill_pick(int *w, int *h);

static const char *SIDEFILL_VS =
    "#version 330\n"
    "void main(){ vec2 p = vec2((gl_VertexID<<1)&2, gl_VertexID&2); gl_Position = vec4(p*2.0-1.0,0.0,1.0); }\n";
static const char *SIDEFILL_FS =
    "#version 330\n"
    "uniform sampler2D u_pic; uniform sampler2D u_art;\n"
    "uniform vec4 u_prect; uniform vec2 u_win; uniform vec4 u_pcover; uniform vec4 u_acover;\n"
    "uniform int u_mode; uniform float u_feather; out vec4 frag;\n"
    "void main(){\n"
    "  vec2 p = gl_FragCoord.xy;\n"
    "  if (p.x >= u_prect.x && p.x < u_prect.z && p.y >= u_prect.y && p.y < u_prect.w) discard;\n"
    "  vec2 uv = p / u_win;\n"
    "  vec2 cu = u_pcover.xy + uv * u_pcover.zw;\n"
    "  vec3 bl = vec3(0.0);\n"
    "  for (int i = 0; i < 32; i++) {\n"
    "    float r = 0.045 * sqrt((float(i) + 0.5) / 32.0), a = float(i) * 2.399963;\n"
    "    bl += texture(u_pic, clamp(cu + vec2(cos(a) * r * 0.75, sin(a) * r), 0.0, 1.0)).rgb;\n"
    "  }\n"
    "  bl *= (0.5 / 32.0);\n"
    "  vec3 c = bl;\n"
    "  if (u_mode == 2) {\n"
    "    float d = max(max(u_prect.x - p.x, p.x - u_prect.z), max(u_prect.y - p.y, p.y - u_prect.w));\n"
    "    vec3 art = texture(u_art, u_acover.xy + uv * u_acover.zw).rgb * 0.85;\n"
    "    c = mix(bl, art, smoothstep(0.0, u_feather, d));\n"
    "  }\n"
    "  frag = vec4(c, 1.0); }\n";

static GLuint s_sf_prog = 0, s_sf_pic = 0, s_sf_art = 0;
static int    s_sf_pw = 0, s_sf_ph = 0, s_sf_art_tried = 0;
static void *s_sf_ctx = NULL;
static const char *s_sf_names[8] = { "u_pic", "u_art", "u_prect", "u_win", "u_pcover", "u_acover", "u_mode", "u_feather" };
static GLint  s_sf_u[9];

/* uv window (offset.xy, scale.zw) that cover-scales an iw x ih image onto the ww x wh window, centred */
static void sidefill_cover(int iw, int ih, int ww, int wh, float *o) {
    float s = (float)ww / (float)iw;
    if ((float)wh / (float)ih > s) s = (float)wh / (float)ih;
    o[2] = (float)ww / ((float)iw * s); o[3] = (float)wh / ((float)ih * s);
    o[0] = 0.5f - o[2] * 0.5f; o[1] = 0.5f - o[3] * 0.5f;
}

static void sidefill_tex_params(GLuint t) {
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void psxrt_sidefill(int ww, int wh) {
    static int art_w, art_h;
    int mode = psxrt_sidefill_mode();
    int lx, ly, lw, lh;
    if (!mode || !s_present_vao) return;
    letterbox_rect(ww, wh, &lx, &ly, &lw, &lh);
    if (lw < 16 || lh < 16 || (ww - lw < 4 && wh - lh < 4)) return;   /* picture fills the window (bars under ~4px): no fill */
    if (s_sf_prog == (GLuint)-1) return;
    { void *cx = SDL_GL_GetCurrentContext(); if (cx != s_sf_ctx) {   /* new GL context: every id from the old one is dead */
        if (s_sf_ctx) { s_sf_prog = s_sf_pic = s_sf_art = 0; s_sf_pw = s_sf_ph = 0; s_sf_art_tried = 0; }
        s_sf_ctx = cx; } }
    if (!s_sf_prog) { s_sf_prog = build_program(SIDEFILL_VS, SIDEFILL_FS); if (!s_sf_prog) { s_sf_prog = (GLuint)-1; return; }
        for (int i = 0; i < 8; i++) s_sf_u[i] = p_glGetUniformLocation(s_sf_prog, s_sf_names[i]); }
    if (mode == 2 && !s_sf_art_tried) {   /* one random PNG per boot; none usable -> blur */
        int w = 0, h = 0;
        unsigned char *px = psxrt_sidefill_pick(&w, &h);
        s_sf_art_tried = 1;
        if (px) {   /* flip to bottom-up, as the window and the picture copy */
            unsigned char *row = (unsigned char *)malloc((size_t)w * 4);
            for (int y = 0; row && y < h / 2; y++) {
                unsigned char *a = px + (size_t)y * w * 4, *b = px + (size_t)(h - 1 - y) * w * 4;
                memcpy(row, a, (size_t)w * 4); memcpy(a, b, (size_t)w * 4); memcpy(b, row, (size_t)w * 4);
            }
            free(row);
            glGenTextures(1, &s_sf_art);
            p_glActiveTexture(PSXGL_TEXTURE0 + 1);
            sidefill_tex_params(s_sf_art);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
            art_w = w; art_h = h;
            free(px);
            fprintf(stderr, "[psxrt-sidefill] art %dx%d\n", w, h);
        } else {
            fprintf(stderr, "[psxrt-sidefill] no usable PNG in PSXRT_SIDEFILL_DIR: blur\n");
        }
    }
    if (mode == 2 && !s_sf_art) mode = 1;
    p_glActiveTexture(PSXGL_TEXTURE0);
    if (!s_sf_pic) glGenTextures(1, &s_sf_pic);
    glBindTexture(GL_TEXTURE_2D, s_sf_pic);
    if (s_sf_pw != lw || s_sf_ph != lh) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, lw, lh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        sidefill_tex_params(s_sf_pic);
        s_sf_pw = lw; s_sf_ph = lh;
    }
    p_glBindFramebuffer(PSXGL_READ_FRAMEBUFFER, 0);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lx, ly, lw, lh);
    p_glBindFramebuffer(PSXGL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    glViewport(0, 0, ww, wh);
    p_glUseProgram(s_sf_prog);
    p_glUniform1i(s_sf_u[0], 0);
    p_glUniform1i(s_sf_u[1], 1);
    if (mode == 2) { p_glActiveTexture(PSXGL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, s_sf_art); p_glActiveTexture(PSXGL_TEXTURE0); }
    {
        float pc[4], ac[4] = { 0, 0, 1, 1 };
        sidefill_cover(lw, lh, ww, wh, pc);
        if (mode == 2) sidefill_cover(art_w, art_h, ww, wh, ac);
        p_glUniform4f(s_sf_u[2], (float)lx, (float)ly, (float)(lx + lw), (float)(ly + lh));
        p_glUniform2f(s_sf_u[3], (float)ww, (float)wh);
        p_glUniform4f(s_sf_u[4], pc[0], pc[1], pc[2], pc[3]);
        p_glUniform4f(s_sf_u[5], ac[0], ac[1], ac[2], ac[3]);
    }
    p_glUniform1i(s_sf_u[6], mode);
    p_glUniform1f(s_sf_u[7], 0.12f * (float)(wh < ww ? wh : ww));
    p_glBindVertexArray(s_present_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    p_glBindVertexArray(0);
    p_glUseProgram(0);
}
