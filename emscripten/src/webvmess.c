#include <stdlib.h>
#include <string.h>
#include "z_hooks.h"
#include "webvmess.h"

static int vmess_args_count = 0;
static int vmess_args_size = 0;
static t_atom *vmess_args = 0;
static char *vmess_selector;

void webpdl2ork_parse_vmess(va_list ap, const char *fmt) {
    vmess_args_count = 1;

    int fmt_len = strlen(fmt);
    for (int i = 0; i < fmt_len; i++)
        if (fmt[i] == 'i' || fmt[i] == 's')
            vmess_args_count++;
    
    vmess_args_size = vmess_args_count;
    vmess_args = calloc(vmess_args_size, sizeof(t_atom));

    for (int i = 0, arg = 0; i < fmt_len; i++) {
        char type = fmt[i];
        switch (type) {
            case 'x':
                (void)va_arg(ap, void*); // consume and ignore
                break;
            case 'i':
                arg++;
                SETFLOAT(&vmess_args[arg], (t_float)va_arg(ap, int));
                break;
            case 's':
                arg++;
                SETSYMBOL(&vmess_args[arg], gensym(va_arg(ap, const char *)));
                break;
            default:
                fprintf(stderr, "[webpdl2ork_vmess] unsupported format char: '%c'\n", type);
                break;
        }
    }
}

void webpdl2ork_vmess(void *x, t_vmess_resolver resolve_cbuf, const char *sel, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if(x == NULL) {
        va_end(ap);
        return;
    }

    webpdl2ork_parse_vmess(ap, fmt);
    va_end(ap);

    const char *vmess_cbuf = resolve_cbuf(x);

    (*LIBPDSTUFF->i_hooks.h_messagehook)(vmess_cbuf, sel, vmess_args_count, vmess_args);
    vmess_args_size = 0;
    vmess_args_count = 0;
    free(vmess_args);
    free(vmess_cbuf);
}

void webpdl2ork_start_vmess(const char *sel, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    webpdl2ork_parse_vmess(ap, fmt);
    va_end(ap);

    vmess_selector = calloc(strlen(sel) + 1, sizeof(char));
    strcpy(vmess_selector, sel);
}

void webpdl2ork_end_vmess(void *x, t_vmess_resolver resolve_cbuf) {
    const char *vmess_cbuf = resolve_cbuf(x);

    (*LIBPDSTUFF->i_hooks.h_messagehook)(vmess_cbuf, vmess_selector, vmess_args_count, vmess_args);
    vmess_args_size = 0;
    vmess_args_count = 0;
    free(vmess_args);
    free(vmess_selector);
    free(vmess_cbuf);
}

void webpdl2ork_s(const char *s) {
    if(vmess_args_size == vmess_args_count) {
        vmess_args_size *= 2;
        vmess_args = realloc(vmess_args, vmess_args_size * sizeof(t_atom));
    }

    SETSYMBOL(&vmess_args[vmess_args_count], gensym(s));
    vmess_args_count++;
}

void webpdl2ork_f(t_float f) {
    if(vmess_args_size == vmess_args_count) {
        vmess_args_size *= 2;
        vmess_args = realloc(vmess_args, vmess_args_size * sizeof(t_atom));
    }

    SETFLOAT(&vmess_args[vmess_args_count], f);
    vmess_args_count++;
}