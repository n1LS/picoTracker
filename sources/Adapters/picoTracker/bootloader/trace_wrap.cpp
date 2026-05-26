/*
 * Bootloader-local wrappers to drop Trace log calls from linked libraries.
 *
 * These wrappers are used with -Wl,--wrap on the mangled C++ symbols for
 * Trace::Debug and Trace::Log.
 */

extern "C" void __wrap__ZN5Trace5DebugEPKcz(const char *fmt, ...) { (void)fmt; }

extern "C" void __wrap__ZN5Trace3LogEPKcS1_z(const char *category,
                                             const char *fmt, ...) {
  (void)category;
  (void)fmt;
}
