/*
 * Minimal, self-contained reproduction of the melee mpColl_80046904 residual
 * class (GC/1.2.5n, -O4,p): a used-once scalar parameter whose home web will
 * NOT promote out of the parameter stratum.
 *
 * Compiled with the melee cflags, `test_repro` reproduces the mpcoll baseline
 * register assignment EXACTLY:
 *   coll(c)=r22, flags=r23, stay=r24, plat=r25, then the six loop-carried
 *   locals at r26..r31 (stmw r22).
 * i.e. the two parameters home to the two LOWEST callee-saved registers.
 *
 * The mpColl_80046904 DOL instead homes `coll`->r26 and `flags`->r27 with the
 * identical instruction stream. `CollData* c = coll` (pointer) promotes coll to
 * the local band and matches it (99.68%); every scalar form of `u32 fl = flags`
 * is aliased at initial lowering (pcode-*-initial object count delta +0) or, as
 * a one-field aggregate, spilled to memory. So this fixture isolates the open
 * question: what stream-preserving source shape numbers a used-once scalar
 * parameter home into the function-scope-local vreg stratum?
 *
 * Build:
 *   <melee>/build/compilers/GC/1.2.5n/mwcceppc.exe -O4,p -proc gekko \
 *     -fp hardware -enum int -inline auto ... -c mpcoll_scalar_param_promotion.c
 * Inspect: powerpc-eabi-objdump -dr --disassemble=test_repro
 */
typedef unsigned int u32;
typedef int bool;
struct C {
    int x;
    float f;
    int a, b, c2, d;
};
extern int leftw(struct C*);
extern int rightw(struct C*);
extern void squeeze(struct C*, int);
extern int ceil_(struct C*, int);

bool test_repro(struct C* c, u32 flags)
{
    int prevb;
    int sqz;
    int oldsqz;
    int sqzall;
    int lr;
    bool touched;
    u32 fl = flags; /* scalar copy -- aliased/folded, does not promote */
    bool plat;
    bool stay;
    plat = fl & 2;
    stay = fl & 1;
    lr = 0;
    touched = 0;
    sqzall = 0;
    sqz = 0;
    do {
        oldsqz = sqz;
        prevb = c->a;
        sqz = 0;
        if (leftw(c)) {
            lr |= 1;
            sqz |= 8;
        }
        if (rightw(c)) {
            lr |= 2;
            sqz |= 4;
        }
        if (ceil_(c, lr))
            sqz |= 1;
        if ((lr & 3) == 3)
            squeeze(c, lr);
        if (plat && stay)
            touched = 1;
        sqzall |= sqz;
    } while (prevb != c->b || sqz != oldsqz);
    if (!touched && (fl & 4)) /* the single, late, cross-call use of flags */
        touched = 1;
    if (!(sqzall & 8))
        c->c2 = 0;
    if (!(sqzall & 4))
        c->d = 0;
    return touched;
}
