# Add a structural ray-tracing host bridge and generated-raygen canary

This ExecPlan is a living document. The sections Progress, Surprises and Discoveries, Decision Log,
and Outcomes and Retrospective must be kept up to date as work proceeds.

This plan follows `.agents/PLANS.md` from the SlangPy repository root.

## Purpose / Big Picture

Slang's structural ray-tracing API declares hit, miss, and callable groups in a typed
`ITraceProgramLayout`. SlangPy currently accepts only manually named legacy shader entry points and
hit groups. After this change, a SlangPy ray-tracing call can name one structural trace-program
layout, and the host bridge will reflect that layout, resolve each synthesized stage with its native
shader stage, preserve every explicit shader-binding-table slot, link those stages with SlangPy's
generated `raygen_main`, and dispatch through the existing D3D12, Vulkan, or CUDA ray-tracing
pipeline path.

The visible proof is a small triangle canary. A generated SlangPy ray-generation shader traces rays
through a structural layout and writes exact expected hit and miss colors. The existing legacy
ray-tracing canary remains unchanged and passing.

This plan covers Falcor port phases 0 and 1 only. It does not port a Falcor renderer, add Metal
runtime ray tracing, add hardware linear-swept-sphere support, preserve shader execution reordering,
or solve the deferred problem of combining several payload-typed structural layouts into one
physical pipeline.

## Progress

- [x] (2026-09-02 22:55Z) Confirmed `kaizhangNV/slangpy` exists and the GitHub CLI is authenticated.
- [x] (2026-09-02 22:55Z) Created SlangPy branch `codex/structural-rt-host-bridge` at
  `1c0dddde0b86419aca16cf6b179ac2c9f540aba7`, with the fork as `origin` and
  `shader-slang/slangpy` as `upstream`.
- [x] (2026-09-02 22:55Z) Created Falcor branch `codex/structural-rt-port` at
  `046545b1d3dac23e9ba1a75498eb75f6c9280dfc` and initialized its missing pinned data,
  MaterialX, and OpenPBR submodules.
- [x] (2026-09-03 07:04Z) Rebuilt and verified structural Slang at `b0f010593...`, configured
  SlangPy against that source and its matching Release artifacts, completed a capped Debug build,
  imported the in-tree package with CPython 3.12, and passed the unchanged Vulkan/CUDA device and
  legacy ray-tracing canaries (4 tests).
- [x] (2026-09-03 16:24Z) Implemented and tested stage-aware checked entry-point lookup in native
  SGL, including nested composed-module resolution, ambiguity rejection, and explicit actual-stage
  verification.
- [x] (2026-09-03 16:24Z) Implemented and tested lifetime-safe structural trace-layout snapshots for
  hit, miss, and callable groups.
- [x] (2026-09-03 16:24Z) Implemented and tested conversion from one reflected structural layout to
  existing SGL pipeline and sparse shader-table descriptors. The native test passes 69 assertions,
  including reversed declaration order, holes, trailing minimum counts, invalid slots, and
  unsupported non-empty records.
- [x] (2026-09-03 16:24Z) Exposed the bridge through nanobind and added the single-layout
  `trace_program_layout` SlangPy functional option, mutually exclusive with legacy group lists and
  included in deterministic pipeline/cache identity.
- [x] (2026-09-03 16:24Z) Added and ran the generated-raygen structural triangle canary while keeping
  the legacy canary passing. Both tests dispatch successfully on Linux Vulkan and CUDA.
- [x] (2026-09-03 00:59Z) Completed the final capped Linux Debug rebuild, generated the nanobind
  API stub, passed pre-commit and pyright with no findings, passed the focused native bridge test
  (1 test, 131 assertions), passed all 197 native SGL tests in three sequential bounded shards, and
  passed the Python configuration and Vulkan/CUDA legacy-plus-structural suites (13 tests total).
- [ ] Run the phase-one canary on available local Windows, Linux, and macOS lanes, recording any
  compile-only backend limitation explicitly.
- [ ] Commit and push the SlangPy implementation, update the Falcor submodule pin, and record both
  commits and validation evidence in Falcor's change ledger.

## Surprises and Discoveries

- Observation: `external/slangpy` began as a detached submodule checkout with only the upstream
  remote, although the user's `kaizhangNV/slangpy` fork already existed.
  Evidence: the baseline checkout reported detached HEAD at `1c0dddde...`; `gh repo view` resolved
  the existing fork. The branch and two-remote arrangement are now established locally.

- Observation: the exact clean structural Slang checkout is
  `/home/zhangkai/Documents/slangwork/slang-core-ecosys/another-slang-rt-recovery` at
  `b0f010593568239005df17c30ea875c0edf25049`; the similarly named `another-slang` checkout is on a
  different dirty revision and must not be used accidentally.
  Evidence: `git rev-parse HEAD` and `git status --short --branch` were recorded for both checkouts.

- Observation: the unmodified optimized SlangPy build reaches the nanobind extension link and then
  GCC 11.5 crashes inside LTO while compiling `src/slangpy_ext/math/vector.cpp`; the same source
  completes all 392 Debug build steps.
  Evidence: the Release link reports an internal compiler error in `lto1`/`sched2`, while the
  explicitly capped `linux-gcc-debug` build links `libsgl.so`, `slangpy_ext`, examples, and
  `sgl_tests`. Phase 1 therefore uses Debug locally and treats the Release failure as a baseline
  toolchain issue.

- Observation: the exact structural compiler build initially contained stale cached version and
  standard-module paths even though the source checkout was correct.
  Evidence: after clearing `SLANG_VERSION_*` and `SLANG_STANDARD_MODULE_*` cache entries and
  rebuilding the requested targets, `slangc -version` reports `2026.16-90-gb0f010593`, and the
  compiler, GLSL support libraries, and `slang-standard-module-2026.16` all carry the matching
  version.

- Observation: the unchanged legacy SlangPy canary executes on both enabled Linux backends.
  Evidence: `test_enumerate_adapters` and `test_raytracing` each passed for Vulkan and CUDA under
  CPython 3.12, for a total of four tests in 3.10 seconds.

- Observation: the first structural CUDA dispatch exposed a Slang compiler target-lowering defect,
  not a host-bridge or shader-API design gap. Portable structural stage-input lowering synthesized
  triangle/custom attribute parameters after the normal entry-point input canonicalization pass;
  CUDA varying legalization therefore converted uses into invalid pointer field access or
  pointer-to-pointer calls.
  Evidence: the failure reproduced directly with Slang's existing portable attribute tests and
  NVRTC. Compiler commit `7b2bf16a65406ad4fc5973b78c05bc044e57dc24` reruns canonical input
  translation after late structural lowering and adds four PTX/NVRTC lanes. The complete portable
  structural target folder then passed 76/76 tests, and the SlangPy CUDA canary dispatched with the
  expected four exact colors.

- Observation: stage-aware materialization succeeds without a new public Slang reflection API for
  the controlled Phase 1 module graph.
  Evidence: the adapter recursively enumerates SGL's composed-module leaves, accepts exactly one
  leaf for which `findAndCheckEntryPoint(name, stage)` succeeds, rejects ambiguity, and passes both
  the native composed-module tests and the generated-raygen canary.

- Observation: qualified structural stage types need one deterministic target-safe physical name
  shared by reflection, direct checked materialization, and automatic synthesis from a trace call.
  Evidence: compiler commit `8bc787db46d61f3816528a5eb08709a379074d54` preserves ordinary safe
  names, encodes qualified/reserved/unsafe names, preserves explicit renames, and passes the two
  focused unit tests plus all 78 portable structural target tests.

- Observation: the monolithic native SGL test process contains an intermittent pre-existing
  exception-safety trap in `cache_writer_flush_waits_for_reserved_job`: an exception before its
  promise is released can leave a future destructor waiting indefinitely. Phase 1 changes none of
  the cache-writer, persistent-cache, watcher, or hot-reload code.
  Evidence: one monolithic CTest run and one direct run stalled at that test, while the test alone,
  the exact preceding hot-reload sequence, ten stress repetitions, a later monolithic CTest run,
  and all three sequential native shards passed. Phase 1 acceptance therefore uses bounded,
  sequential hot-reload, persistent-cache, and remaining-suite processes.

- Observation: nanobind API stub generation succeeds, while the separate `slangpy_pydoc` source
  documentation extraction target cannot run in the baseline configuration because its selected
  system Python lacks the optional `pybind11_mkdoc` package. This is an environment dependency, not
  a generated binding or stub failure.

- Observation: slang-rhi indexes stage exports and hit-group exports in one name map, so a fixed
  generated hit-group prefix alone is not collision-proof against legal user stage names.
  Evidence: independent review traced the shared lookup on Vulkan, D3D12, and CUDA. The adapter now
  reserves every reflected stage export plus SlangPy's `raygen_main` before choosing deterministic
  hit-group names, and the native regression forces the formerly colliding spelling.

## Decision Log

- Decision: implement support for exactly one `ITraceProgramLayout` per SlangPy ray-tracing call in
  phase 1.
  Rationale: it proves the structural path without inventing the deferred public contract for
  multiple payload types in one physical pipeline.
  Date/Author: 2026-09-02 / Codex with user direction.

- Decision: retain the existing native `RayTracingPipelineDesc` and `ShaderTableDesc` path.
  Rationale: D3D12, Vulkan, and CUDA already consume ordinary entry points, native hit-group
  descriptions, and shader-table arrays. Structural reflection should adapt into those inputs.
  Date/Author: 2026-09-02 / Codex.

- Decision: preserve explicit reflected slot numbers, including internal holes, instead of using
  declaration or reflection order.
  Rationale: a trace call indexes the physical shader binding table numerically; compacting sparse
  slots changes behavior even when compilation succeeds.
  Date/Author: 2026-09-02 / Codex.

- Decision: validate synthesized stage materialization with the canary before changing Slang's
  public compiler API.
  Rationale: current reflection returns a stage name, native stage, and type, while
  `IModule::findAndCheckEntryPoint(name, stage)` can materialize a checked entry point from a known
  declaring module. A direct compiler API is justified only if the controlled module graph cannot
  be resolved robustly.
  Date/Author: 2026-09-02 / Codex.

- Decision: repair the CUDA attribute failure in Slang's canonical pass ordering rather than add a
  shader workaround to the canary.
  Rationale: deriving barycentrics manually would hide a general defect that also affects custom
  hit attributes. Normalizing late synthesized inputs restores the invariant expected by all target
  varying legalizers and is covered by reusable compiler tests.
  Date/Author: 2026-09-03 / Codex.

## Outcomes and Retrospective

Phase 0 and the local Linux implementation portion of Phase 1 are complete. The bridge reflects one
structural layout, resolves synthesized stages by name and native stage, preserves sparse SBT slots,
and reuses the existing pipeline and generated-raygen path. After the companion compiler fix, the
native bridge passes 131/131 assertions, all 197 native SGL tests pass in bounded sequential shards,
the focused Python configuration/runtime suite passes 13/13, the portable structural compiler
suite passes 78/78, and legacy plus structural dispatch succeeds on both Vulkan and CUDA.
Cross-platform worker execution, commits, and the Falcor submodule/report update remain in progress.

## Context and Orientation

The SlangPy repository in this plan is the Falcor submodule at `external/slangpy`. Its native SGL
layer lives under `src/sgl`, its nanobind extension under `src/slangpy_ext`, its Python functional
API under `slangpy`, and tests under `tests` and `slangpy/tests`.

A shader binding table, abbreviated SBT, is the runtime array that maps numeric ray-tracing indices
to miss, hit-group, and callable shader records. A structural trace-program layout is a shader type
implementing `rt::ITraceProgramLayout`; it declares the legal group types and each group's explicit
numeric slot. A synthesized structural stage is a compiler-generated native ray-tracing entry point
for a stage struct such as closest-hit or miss. Unlike a legacy function already marked with a
`shader` attribute, it must be resolved by both its name and intended native stage.

The current SlangPy functional ray-tracing path begins in `slangpy/core/function.py`, builds generated
raygen source and link components in `slangpy/core/calldata.py`, and uses the native shader and
pipeline wrappers in `src/sgl/device/shader.h`, `src/sgl/device/shader.cpp`, and related pipeline
files. Existing ray-tracing tests are the behavioral baseline and must remain supported.

The compiler checkout used by this work is
`/home/zhangkai/Documents/slangwork/slang-core-ecosys/another-slang-rt-recovery` at commit
`8bc787db46d61f3816528a5eb08709a379074d54`, based on the original structural checkpoint
`b0f010593568239005df17c30ea875c0edf25049`. SlangPy must use both headers and libraries from that
same checkout. Substituting only a shared library could create an experimental API/ABI mismatch.

## Plan of Work

First, make the baseline reproducible. Confirm that the structural compiler build artifacts match
the pinned source commit, configure SlangPy against that local source and build directory, enable
Slang experimental features only in affected device sessions, and run a small unchanged import or
device test. Record all commands and outputs in this document and in Falcor's
`reports/structural-rt-port-checklist.md`.

Second, extend native SGL reflection. Add value-owning wrappers that copy a trace-program layout's
trace-context metadata and its miss, hit, and callable groups. Each copied stage needs its reflected
entry-point name, native stage, and enough module identity to resolve it. Add stage-aware checked
entry-point lookup using `slang::IModule::findAndCheckEntryPoint`. Preserve existing
`findEntryPointByName` behavior for legacy shader functions.

Third, add one reusable adapter that turns a reflected structural layout into the same native inputs
used by legacy ray tracing. It must create hit-group descriptions, stage-aware link components, and
miss/hit/callable name arrays indexed by the reflected numeric slots. It must preserve sparse holes,
reject negative or duplicate slots defensively, and produce actionable diagnostics when a stage
cannot be materialized. Non-empty shader-record data is outside phase 1 and must be rejected clearly
rather than silently ignored.

Fourth, expose the single-layout option through nanobind and `FunctionNode.ray_tracing`. Structural
and legacy group arguments must be mutually exclusive. The selected layout and every pipeline-
affecting option must participate in functional-call and pipeline cache identity. Reuse SlangPy's
existing generated `raygen_main`, call-data marshalling, root shader-object binding, pipeline cache,
and dispatch path.

Finally, add the minimal structural triangle canary. It must exercise a generated raygen linked to
synthesized miss and closest-hit stages and compare exact output values, including both hit and miss
rays. Add focused native tests for reflection, checked lookup, negative/duplicate slots, sparse slots,
and declaration-order independence. Build before testing as required by `AGENTS.md`, run pre-commit,
and then validate the same snapshot on available local runners.

## Concrete Steps

All native builds on Linux run through the descendant-process limiter and pass an explicit limit of
eight jobs. Configure commands also set the environment limits because dependency probes or Python
build helpers may compile code.

From the structural Slang repository, verify the compiler identity and existing build before using
it:

    cd /home/zhangkai/Documents/slangwork/slang-core-ecosys/another-slang-rt-recovery
    git rev-parse HEAD
    build/Release/bin/slangc -version

From the SlangPy repository, configure and build against that exact local compiler. The final preset
and build directory will be recorded after inspecting the repository's current presets and local
Slang options. The build invocation must have this shape:

    cd /home/zhangkai/Documents/slangwork/slang-core-ecosys/falcor2/external/slangpy
    CMAKE_BUILD_PARALLEL_LEVEL=8 MAX_JOBS=8 \
      /home/zhangkai/.codex/skills/limit-cpp-build-parallelism/scripts/run-limited-build.sh \
      cmake --build --preset <linux-debug-preset> --parallel 8

Run the focused native and Python tests only after the build. Test commands that can trigger a build
or compile shaders also run under the same limiter and expose at most eight workers. Record exact
test paths and expected values once the baseline test and new canary locations are finalized.

Before completion, run:

    cd /home/zhangkai/Documents/slangwork/slang-core-ecosys/falcor2/external/slangpy
    pre-commit run --all-files
    git diff --check

Run local-worker validation through the external recipe stored under
`~/.codex/local-build-farm/projects/`; do not commit worker configuration to either repository.

## Validation and Acceptance

Phase 0 is accepted when the repository and all pinned submodules are initialized, both branches and
remotes are reproducible, the compiler source/header/library identity is logged, SlangPy builds
against that exact compiler with the eight-job cap, and an unchanged baseline import/device test
passes.

Phase 1 is accepted when the legacy ray-tracing canary still passes and the new structural canary
uses a SlangPy-generated `raygen_main` to dispatch at least one hit ray and one miss ray with exact
expected output. Focused native tests must prove checked stage lookup and exact sparse-slot
placement. Negative and duplicate slot inputs must fail with clear diagnostics. The available
Windows D3D12, Vulkan, and CUDA lanes should execute the canary where supported. macOS initially
proves structural Metal compilation/code generation because the pinned Metal RHI does not implement
pipeline ray-tracing dispatch.

The implementation is not complete if it hard-codes the canary's group names in Python, compacts
sparse slots, relies on reflection enumeration order, mixes legacy and structural shader APIs into
one compiled program, or leaves SlangPy linked to headers and a library from different compiler
revisions.

## Idempotence and Recovery

Configure and test steps are repeatable. Do not delete shared build directories. If the local Slang
identity is wrong, create a separate correctly configured build directory or repair it using the
repository's documented build workflow; never replace only the runtime library. If a remote worker
fails, preserve its log and rerun from a fresh disposable worker snapshot after fixing the canonical
Linux workspace.

All source edits occur in the Linux SlangPy and Falcor branches. Worker checkouts never commit or
push. If the structural adapter proves that current reflection cannot identify the declaring module
unambiguously, stop that approach, record the failing canary and diagnostic here, and add the
smallest principled compiler reflection/materialization API in the pinned Slang checkout.

## Artifacts and Notes

Falcor's cross-repository status and validation ledger is
`reports/structural-rt-port-checklist.md` relative to the Falcor root. Append exact commands, runner
names, backend results, logs, and commit pairs there as evidence is produced.

The canonical source revisions at the start of the work are:

    Falcor:  046545b1d3dac23e9ba1a75498eb75f6c9280dfc
    SlangPy: 1c0dddde0b86419aca16cf6b179ac2c9f540aba7
    Slang:   b0f010593568239005df17c30ea875c0edf25049

The Phase 1 compiler dependency is:

    Slang: 8bc787db46d61f3816528a5eb08709a379074d54
           (includes 7b2bf16a65406ad4fc5973b78c05bc044e57dc24)
           kaizhangNV/slang, branch codex/structural-rt-cuda-hit-attributes

## Interfaces and Dependencies

The native interface is the distinct `SlangModule::checked_entry_point(name, ShaderStage,
conformances)` method,
`ProgramLayout::find_trace_program_layout(name)`, the `TraceProgram*Info` value snapshots, and
`create_structural_ray_tracing_bindings`. The Python-native convenience method is
`SlangModule.structural_ray_tracing_bindings(...)`; the high-level functional option is
`FunctionNode.ray_tracing(trace_program_layout=...)`.

The Python interface will add one optional, typed parameter to `FunctionNode.ray_tracing`, initially
spelled `trace_program_layout: str | None`. Supplying it together with legacy `hit_groups`,
`miss_entry_points`, `hit_group_names`, or `callable_entry_points` must raise a clear error. Existing
legacy callers must observe no behavior change.

Revision note, 2026-09-02: created the living plan at the start of implementation to capture the
branch setup, compiler identity constraint, initial architecture, validation gates, and explicitly
deferred features before source changes begin.

Revision note, 2026-09-03: recorded the exact compiler repair, successful capped Debug baseline,
unchanged Vulkan/CUDA canary results, and the unrelated Release LTO compiler crash.

Revision note, 2026-09-03: recorded the implemented bridge interfaces, local validation, discovered
CUDA attribute-lowering defect and companion compiler fix, and the remaining cross-platform and
publication work.

Revision note, 2026-09-03: recorded final compiler commit `8bc787db4`, deterministic structural
stage naming, final Linux validation, the bounded native-test sharding gate, and the optional
documentation-extraction dependency distinction.
