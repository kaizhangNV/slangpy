# Add a structural ray-tracing host bridge and generated-raygen canary

This ExecPlan is a living document. The sections Progress, Surprises and Discoveries, Decision Log,
and Outcomes and Retrospective must be kept up to date as work proceeds.

This plan follows `.agents/PLANS.md` from the SlangPy repository root.

## Purpose / Big Picture

Slang's structural ray-tracing API declares hit, miss, and callable groups in a typed
`ITraceProgramLayout`. Before this change, SlangPy accepted only manually named legacy shader entry
points and hit groups. With the implemented bridge, a SlangPy ray-tracing call can name one
structural trace-program layout. The bridge reflects that layout, resolves each synthesized stage
with its native shader stage, preserves every explicit shader-binding-table slot, links those stages
with SlangPy's generated `raygen_main`, and dispatches through the existing D3D12, Vulkan, or CUDA
ray-tracing pipeline path.

The visible proof is a small triangle canary. A generated SlangPy ray-generation shader traces rays
through a structural layout and checks four specified hit/miss corner values with
`numpy.allclose(..., atol=0.01)` (and NumPy's default `rtol`). The existing legacy ray-tracing
canary remains unchanged and passing.

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
- [x] (2026-09-02) Rebuilt and verified structural Slang at `b0f010593...`, configured
  SlangPy against that source and its matching Release artifacts, completed a capped Debug build,
  imported the in-tree package with CPython 3.12, and passed the unchanged Vulkan/CUDA device and
  legacy ray-tracing canaries (4 tests).
- [x] (2026-09-02 23:24Z) Implemented and tested stage-aware checked entry-point lookup in native
  SGL, including nested composed-module resolution, ambiguity rejection, and explicit actual-stage
  verification.
- [x] (2026-09-02 23:24Z) Implemented and tested lifetime-safe structural trace-layout snapshots for
  hit, miss, and callable groups.
- [x] (2026-09-02 23:24Z) Implemented and tested conversion from one reflected structural layout to
  existing SGL pipeline and sparse shader-table descriptors. The final focused native test passes
  131 assertions, including reversed declaration order, holes, trailing minimum counts, invalid
  slots, collision-safe hit-group names, ownership, and unsupported non-empty records.
- [x] (2026-09-02 23:24Z) Exposed the bridge through nanobind and added the single-layout
  `trace_program_layout` SlangPy functional option, mutually exclusive with legacy group lists and
  included in deterministic pipeline/cache identity.
- [x] (2026-09-02 23:24Z) Added and ran the generated-raygen structural triangle canary while keeping
  the legacy canary passing. Both tests dispatch successfully on Linux Vulkan and CUDA.
- [x] (2026-09-03 00:59Z) Completed the final SlangPy-source capped Linux Debug rebuild, generated the nanobind
  API stub, passed pre-commit and pyright with no findings, passed the focused native bridge test
  (1 test, 131 assertions), passed all 197 native SGL tests in three sequential bounded shards, and
  passed the Python configuration and Vulkan/CUDA legacy-plus-structural suites (13 tests total).
- [x] (2026-09-03 01:05Z) Committed and pushed the SlangPy implementation as
  `c2e73c0b1b0eed0577e544e6abdadfa1d32f7910` on
  `kaizhangNV/slangpy:codex/structural-rt-host-bridge`.
- [x] (2026-09-03 02:30Z) Completed final-SHA local-runner validation. Linux passed the Vulkan and
  CUDA legacy-plus-structural canaries; Windows passed the D3D12, Vulkan, and CUDA pairs; macOS
  passed native/configuration coverage and compiled Slang's compiler-owned structural closest-hit,
  miss, and raygen fixture to non-empty Metal AIR. Metal remains compile-only because the pinned RHI
  has no ray-tracing pipeline, table, or dispatch implementation.
- [x] (2026-09-03 02:36Z) Published final validation documentation and clang-format-only wrapping of
  the native bridge test as `28ee791bc4cb58b071e4d6c873b214dbc2d6a98c`.
- [ ] Publish the Falcor submodule URL/pin and final report/checklist, then record the outer commit in
  Falcor's change ledger.

## Surprises and Discoveries

- Observation: `external/slangpy` began as a detached submodule checkout with only the upstream
  remote, although the user's `kaizhangNV/slangpy` fork already existed.
  Evidence: the baseline checkout reported detached HEAD at `1c0dddde...`; `gh repo view` resolved
  the existing fork. The branch and two-remote arrangement are now established locally.

- Observation: the structural Slang checkout used for this work is
  `/home/zhangkai/Documents/slangwork/slang-core-ecosys/another-slang-rt-recovery`. It started from
  `b0f010593568239005df17c30ea875c0edf25049` and now ends at the published Phase 1 dependency
  `b035d437be74e1ffb6c671c4e6630f07326e300b`; the similarly named `another-slang` checkout is a
  different dirty checkout and must not be used accidentally.
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
  Evidence: the failure reproduced directly with Slang's portable attribute tests and CUDA runtime
  dispatch. Compiler commit `7b2bf16a65406ad4fc5973b78c05bc044e57dc24` reruns canonical input
  translation after late structural lowering and adds four PTX FileCheck lanes. The complete
  portable structural target folder then passed 76/76 tests, and the SlangPy CUDA canary matched
  the four expected corner values within its `atol=0.01` comparison.

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

- Observation: the structural Metal raygen compiler fixture did not terminate with a Release Clang
  compiler even though it completed with GCC and Debug Clang.
  Evidence: `_inlineCandidateOperationCalls()` placed the mutating `inlineCall()` inside
  `SLANG_ASSERT`; Release Clang lowers that assertion to `__builtin_assume` and discards the
  side-effecting operand, so the fixed-point loop selected the same call forever. Compiler commit
  `b035d437be74e1ffb6c671c4e6630f07326e300b` evaluates `inlineCall()` before asserting its result.
  The exact macOS ARM64 Release fixture then generated Metal and non-empty AIR for raygen,
  closest-hit, and miss.

- Observation: descriptive versions depend on the tags visible in the checkout. Fresh-worker
  `slangc -version` output from the fork is `2024.0.7-3799-gb035d437b`, while local `git describe`
  with upstream tags is `v2026.16-93-gb035d437b` for the same source commit.
  Evidence: every acceptance run also verifies the full commit
  `b035d437be74e1ffb6c671c4e6630f07326e300b`; that full SHA, rather than the descriptive version
  string, is the authoritative dependency identity.

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

- Decision: make the Metal adapter inlining mutation unconditional and retain the assertion only as
  a postcondition.
  Rationale: assertion expressions must not carry required side effects; Release compilers are
  permitted to erase them. The two-statement form is portable and preserves the debug invariant.
  Date/Author: 2026-09-03 / Codex.

## Outcomes and Retrospective

The bridge reflects one structural layout, resolves synthesized stages by name and native stage,
preserves sparse SBT slots, and reuses the existing pipeline and generated-raygen path. The source
implementation is published as `c2e73c0b1b0eed0577e544e6abdadfa1d32f7910`. On the final compiler
SHA, Linux passes all 197 native SGL tests in bounded sequential shards, the 9 configuration tests,
and all four legacy-plus-structural Vulkan/CUDA runtime cases. Windows passes all 197 native tests,
the 9 configuration tests, and all six legacy-plus-structural D3D12/Vulkan/CUDA runtime cases. Its
inline control passes compute RayQuery on D3D12/Vulkan, skips CUDA because that device reports ray
queries unsupported, and passes pipeline ray launch on all three backends. macOS passes all 197
native tests and the 9 configuration tests and compiles the compiler-owned structural
raygen/closest-hit/miss fixture to non-empty Metal AIR. The portable structural compiler suite
passed 78/78 before the final Metal-only fix; the final fix separately passes the focused fixture,
all 18 structural Metal tests, and macOS Release Metal/AIR generation. Phase 0 and Phase 1 are
complete; no Falcor renderer source was changed.

The accepted Phase 1 scope is deliberately bounded. It supports one layout per call and concrete,
non-generic stage types declared in one enumerable source-module leaf. Imported-only and generic
stages remain unsupported; the bridge rejects the same fully qualified name in multiple leaves and
does not support one source type reused for several native stages. Non-empty shader records are
rejected. Runtime selection of nonzero sparse slots, actual multi-pipeline cache behavior,
structural hot reload, and Metal runtime dispatch remain explicit follow-up work.

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
`b035d437be74e1ffb6c671c4e6630f07326e300b`, based on the original structural checkpoint
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
and legacy group arguments must be mutually exclusive. The functional-call and pipeline cache
signature must include the structural/legacy mode, layout name, hit-group definitions and names,
miss and callable lists, recursion depth, payload size, attribute size, and pipeline flags. Reuse
SlangPy's existing generated `raygen_main`, call-data marshalling, root shader-object binding,
pipeline cache, and dispatch path.

Finally, add the minimal structural triangle canary. It must exercise a generated raygen linked to
synthesized miss and closest-hit stages and compare the four specified corner values with the
existing `numpy.allclose(..., atol=0.01)` tolerance, including both hit and miss rays. Add focused
native tests for reflection, checked lookup, negative/duplicate slots, sparse slots,
and declaration-order independence. Build before testing as required by `AGENTS.md`, run pre-commit,
and then validate the same snapshot on available local runners.

## Concrete Steps

All native builds on Linux run through the descendant-process limiter and pass an explicit limit of
eight jobs or less. Configure commands also set environment limits because dependency probes or
Python build helpers may compile code. The accepted clean Linux compiler build used four jobs after
GCC 13 failed internally at eight; this lower limit is reproduced below.

From the structural Slang repository, verify and build the exact dependency:

    export SLANG_RT_DIR=/home/zhangkai/Documents/slangwork/slang-core-ecosys/another-slang-rt-recovery
    export BUILD_LIMITER=/home/zhangkai/.codex/skills/limit-cpp-build-parallelism/scripts/run-limited-build.sh
    cd "$SLANG_RT_DIR"
    test "$(git rev-parse HEAD)" = b035d437be74e1ffb6c671c4e6630f07326e300b
    CMAKE_BUILD_PARALLEL_LEVEL=8 MAX_JOBS=8 "$BUILD_LIMITER" \
      cmake --preset default -S . --fresh
    CMAKE_BUILD_PARALLEL_LEVEL=4 MAX_JOBS=4 "$BUILD_LIMITER" \
      cmake --build build --config Release \
      --target slangc slang-glslang slang-glsl-module slang-raytracing-module --parallel 4
    build/Release/bin/slangc -version

The expected version suffix is `gb035d437b`; the full Git SHA check is authoritative. From the
SlangPy repository, configure and build the `linux-gcc` preset against that compiler:

    cd /home/zhangkai/Documents/slangwork/slang-core-ecosys/falcor2/external/slangpy
    python3 -m venv .venv
    .venv/bin/python -m pip install --upgrade pip setuptools wheel
    .venv/bin/python -m pip install 'numpy>=1.26,<3' 'pytest>=8,<9' typing_extensions
    CMAKE_BUILD_PARALLEL_LEVEL=8 MAX_JOBS=8 "$BUILD_LIMITER" \
      cmake --preset linux-gcc -S . --fresh \
      -DSGL_LOCAL_SLANG=ON -DSGL_LOCAL_SLANG_DIR:PATH="$SLANG_RT_DIR" \
      -DSGL_LOCAL_SLANG_BUILD_DIR=build/Release -DPython_ROOT_DIR:PATH="$PWD/.venv" \
      -DSGL_BUILD_EXAMPLES=OFF -DSGL_BUILD_TESTS=ON
    CMAKE_BUILD_PARALLEL_LEVEL=8 MAX_JOBS=8 "$BUILD_LIMITER" \
      cmake --build build/linux-gcc --config Debug \
      --target slangpy_ext sgl_tests slangpy_stub --parallel 8

Run the focused bridge test, the bounded native shards, configuration tests, and both legacy and
structural canaries from the SlangPy root. These are the exact Linux test shapes:

    export PYTHONPATH="$PWD"
    timeout 300s "$BUILD_LIMITER" build/linux-gcc/Debug/sgl_tests \
      --test-case='structural ray tracing native bridge' --no-colors=true
    timeout 300s "$BUILD_LIMITER" build/linux-gcc/Debug/sgl_tests \
      --test-suite=hot_reload --no-colors=true
    timeout 300s "$BUILD_LIMITER" build/linux-gcc/Debug/sgl_tests \
      --test-suite=persistent_cache --no-colors=true
    timeout 900s "$BUILD_LIMITER" build/linux-gcc/Debug/sgl_tests \
      --test-suite-exclude=hot_reload,persistent_cache --no-colors=true
    "$BUILD_LIMITER" .venv/bin/python -m pytest \
      slangpy/tests/slangpy_tests/test_raytracing_config.py -v --device-types nodevice
    "$BUILD_LIMITER" .venv/bin/python -m pytest \
      slangpy/tests/slangpy_tests/test_raytracing.py::test_raytracing \
      slangpy/tests/slangpy_tests/test_raytracing.py::test_structural_raytracing \
      -v --device-types vulkan,cuda
    "$BUILD_LIMITER" .venv/bin/python -m pytest \
      slangpy/tests/device/test_pipeline.py::test_raytrace_simple \
      -v -rs --device-types vulkan,cuda
    .venv/bin/pre-commit run --all-files
    .venv/bin/pyright
    git diff --check

The focused bridge must report one passing test and 131 assertions. The native shards together must
report 197 passing tests, configuration must report 9/9, and the legacy/structural runtime command
must report 4/4. Vulkan compute RayQuery and Vulkan/CUDA pipeline launch pass; CUDA compute RayQuery
skips when the device reports the feature unsupported.

Cross-platform validation uses the external recipe
`~/.codex/local-build-farm/projects/falcor2-structural-rt-phase1.json`. It contains the same source
identity checks plus explicit Windows outer-one/`/MP8` and macOS `--parallel 8` limits. Worker
snapshots never commit or push, and the recipe remains outside both repositories.

## Validation and Acceptance

Phase 0 is accepted when the repository and all pinned submodules are initialized, both branches and
remotes are reproducible, the compiler source/header/library identity is logged, SlangPy builds
against that exact compiler with the eight-job cap, and an unchanged baseline import/device test
passes.

Phase 1 is accepted when the legacy ray-tracing canary still passes and the new structural canary
uses a SlangPy-generated `raygen_main` to dispatch at least one hit ray and one miss ray and match
the four specified corner values under `numpy.allclose(..., atol=0.01)` with NumPy's default
`rtol`. Focused native tests must prove checked stage lookup and exact sparse-slot
placement. Negative and duplicate slot inputs must fail with clear diagnostics. The available
Windows D3D12, Vulkan, and CUDA lanes should execute the canary where supported. macOS initially
proves structural Metal compilation/code generation with Slang's compiler-owned structural fixture
because the pinned Metal RHI does not implement pipeline ray-tracing dispatch; this is not a
SlangPy-canary runtime claim.

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

All source edits occur in the Linux Slang, SlangPy, and Falcor branches. Worker checkouts never
commit or push. If the structural adapter proves that current reflection cannot identify the
declaring module unambiguously, stop that approach, record the failing canary and diagnostic here,
and add the smallest principled compiler reflection/materialization API in the pinned Slang
checkout.

## Artifacts and Notes

Falcor's cross-repository status and validation ledger is
`reports/structural-rt-port-checklist.md` relative to the Falcor root. Append exact commands, runner
names, backend results, logs, and commit pairs there as evidence is produced.

The canonical source revisions at the start of the work are:

    Falcor:  046545b1d3dac23e9ba1a75498eb75f6c9280dfc
    SlangPy: 1c0dddde0b86419aca16cf6b179ac2c9f540aba7
    Slang:   b0f010593568239005df17c30ea875c0edf25049

The Phase 1 compiler dependency is:

    Slang: b035d437be74e1ffb6c671c4e6630f07326e300b
           (includes 7b2bf16a65406ad4fc5973b78c05bc044e57dc24 and
            8bc787db46d61f3816528a5eb08709a379074d54)
           kaizhangNV/slang, branch codex/structural-rt-cuda-hit-attributes

The final acceptance runs and concise outcomes are:

    Linux  20260902-185955: 197 native, 9 config, 4 Vulkan/CUDA canaries passed
    Windows 20260902-191209: 197 native, 9 config, 6 D3D12/Vulkan/CUDA canaries passed;
                              inline control 5 passed, CUDA compute RayQuery skipped
    macOS  20260902-185145: 197 native, 9 config, closest-hit/miss/raygen Metal AIR non-empty

The preserved logs are under
`~/.codex/local-build-farm/runs/falcor2-structural-rt-phase1/<run-id>/<platform>.log`. The macOS
result is compiler-owned Metal fixture coverage, not SlangPy runtime coverage. Falcor's
`reports/structural-rt-port-checklist.md` records assertion counts, artifact byte sizes, log hashes,
worker-only workarounds, and the failed Linux eight-job GCC evidence.

## Interfaces and Dependencies

The native interface is the distinct `SlangModule::checked_entry_point(name, ShaderStage,
conformances)` method,
`ProgramLayout::find_trace_program_layout(name)`, the `TraceProgram*Info` value snapshots, and
`create_structural_ray_tracing_bindings`. The Python-native convenience method is
`SlangModule.structural_ray_tracing_bindings(...)`; the high-level functional option is
`FunctionNode.ray_tracing(trace_program_layout=...)`.

The Python interface adds one optional, typed parameter to `FunctionNode.ray_tracing`, spelled
`trace_program_layout: str | None`. Supplying it together with legacy `hit_groups`,
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

Revision note, 2026-09-03: recorded compiler commits `8bc787db4` and `b035d437b`, deterministic
structural stage naming, the Release-Clang Metal hang repair, final Linux/macOS validation, the
bounded native-test sharding gate, and the optional documentation-extraction dependency
distinction.

Revision note, 2026-09-03: recorded final-SHA Windows D3D12/Vulkan/CUDA validation, the inline
RayQuery control result, the macOS compile-only boundary, and Phase 0-1 completion without Falcor
renderer changes.

Revision note, 2026-09-03: replaced stale command placeholders with the exact final Linux build/test
contract and worker evidence, documented accepted Phase 1 limitations, corrected milestone timing,
and recorded the formatting-only native-test normalization in the validation follow-up.
