# Optimization pipeline

Status legend: **confirmed** means observed in the stock GC/1.2.5 executable;
**reference** means suggested by another version/tool and awaiting exact-binary
verification.

## Frontend

The executable contains `IrOptimizer.c` and trace strings naming these stages:

- `IRO_LoopUnroller`
- `IRO_FindLoops`
- `IRO_CopyAndConstantPropagation`
- `IRO_ConstantFolding`
- `IRO_EvaluateConditionals`
- `IRO_RangePropagateInFNode`
- `IRO_ExpressionPropagation`

These names are **confirmed**. The dispatcher is at `0x0042cd10`; its bytes are
unchanged in GC/1.2.5n. RootCubed's independently versioned GC/1.2.5 debugger
breakpoint at `0x0042cd1f` corroborates the identification.

## Backend

The following pass families are **confirmed** by strings or the external
debugger and still need exact GC/1.2.5 dispatcher verification:

- common-subexpression elimination
- copy propagation
- add propagation
- loop code motion
- loop strength reduction
- loop transforms
- constant propagation
- load deletion
- array-to-register transforms
- code motion
- scheduling
- forward peephole optimization
- register allocation
- prologue/epilogue insertion
- final peephole optimization

Record the exact `-O2`, `-O3`, and `-O4` sequences here once recovered from the
binary. Repetition is semantically important and must not be collapsed.
