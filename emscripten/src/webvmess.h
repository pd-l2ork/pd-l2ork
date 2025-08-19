typedef const char* (*t_vmess_resolver)(void *x);

void webpdl2ork_vmess(void *x, t_vmess_resolver resolve_cbuf, const char *sel, char *fmt, ...);
void webpdl2ork_start_vmess(const char *sel, char *fmt, ...);
void webpdl2ork_end_vmess(void *x, t_vmess_resolver resolve_cbuf);
void webpdl2ork_s(const char *s);
void webpdl2ork_f(t_float f);