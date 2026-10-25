// Exercise the actual portable x87 helpers with optimization enabled.
// See docs/wolf3d-corruption.md for the build and run commands.
#include "dosbox.h"
#undef HAS_LONG_DOUBLE
#include "cpu.h"
#include "paging.h"
#include "cross.h"
#include "fpu.h"
#include "fpu/fpu_instructions.h"
#include <cstdio>

FPU_rec fpu;
unsigned char FPU_ArchitectureType = FPU_ARCHTYPE_387;

// Keep input setup outside the optimizer's view of each instruction.
GCC_ATTRIBUTE(noinline) static void execute_fptan() { FPU_FPTAN(); }
GCC_ATTRIBUTE(noinline) static void execute_fsincos() { FPU_FSINCOS(); }

static bool check(const char *name, void (*instruction)(),
                  double expected_top, double expected_next)
{
    // C0/C3 must survive; C2 must clear; the push must decrement TOP.
    fpu.sw.reg = (7u << 11) | 0x4500;
    fpu.regs[7].d = 0.25;
    instruction();
    const bool ok = unsigned(TOP) == 6 &&
                    (fpu.sw.reg & 0x4700) == 0x4100 &&
                    std::fabs(fpu.regs[6].d - expected_top) < 1e-14 &&
                    std::fabs(fpu.regs[7].d - expected_next) < 1e-14;
    std::printf("%s %s: TOP=%u SW=%04x ST0=%.17g ST1=%.17g\n",
                ok ? "PASS" : "FAIL", name, unsigned(TOP),
                unsigned(fpu.sw.reg), fpu.regs[unsigned(TOP)].d,
                fpu.regs[(unsigned(TOP) + 1) & 7].d);
    return ok;
}

int main()
{
    const bool tangent = check("FPTAN", execute_fptan, 1.0, std::tan(0.25));
    const bool sincos = check("FSINCOS", execute_fsincos,
                             std::cos(0.25), std::sin(0.25));
    return tangent && sincos ? 0 : 1;
}
