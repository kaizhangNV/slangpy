# Migrate SlangPy ray tracing to schema reflection and host-owned shader tables

This ExecPlan is a living document. The sections Progress, Surprises and Discoveries, Decision Log,
and Outcomes and Retrospective must be kept up to date as work proceeds.

This plan follows `.agents/PLANS.md` from the SlangPy repository root.

## Purpose / Big Picture

Slang's revised structural ray-tracing API lets shader authors declare a typed schema describing
which hit, miss, and callable implementations may be used, while the host owns the physical shader
binding table (SBT): its record order, duplicate records, and record bytes. After this migration a
SlangPy caller can select a schema, provide the concrete source shader types for each physical SBT
record, link those compiler-synthesized stages with SlangPy's generated ray-generation shader, and
dispatch through the existing D3D12, Vulkan, or CUDA pipeline. A regression test demonstrates two
payload partitions whose function indices overlap but whose physical records remain distinct.

The outer Falcor repository consumes the bridge for its mini tracer, scene-picking helpers, and
reference path tracer. The reference path tracer puts scattering and visibility payloads in one
schema and uses host-selected physical hit and miss records, which exercises the design change that
motivated this work.

## Progress

- [x] (2026-09-14 19:00Z) Read `.agents/PLANS.md` and the native-build limiter instructions in full.
- [x] (2026-09-14 19:02Z) Created Falcor branch `codex/dynamic-schema-rt-port` and SlangPy branch
  `codex/dynamic-schema-host-bridge` from their clean structural-port heads.
- [x] (2026-09-14 19:00Z) Updated the nested `slang-rhi` URL and gitlink to fork commit
  `5661193d9415fb3c84c068afb149b85ea7fe2310`; its shader table copies application bytes.
- [x] (2026-09-14 19:00Z) Replaced old layout/slot reflection snapshots with value-owning schema, payload-partition, and
  function metadata snapshots.
- [x] (2026-09-14 19:00Z) Adapted a reflected schema plus host-selected source type names and record bytes into native
  pipeline entry points, hit groups, and physical SBT arrays.
- [x] (2026-09-14 19:00Z) Exposed schema selections through nanobind and `FunctionNode.ray_tracing`, preserving the
  legacy API and including all selections and bytes in cache identity.
- [x] (2026-09-14 19:24Z) Added native and Python regression coverage for schema reflection,
  overlapping payload-local indices, closed and open schema entries, repeated/sparse records,
  exact and zero-filled record bytes, invalid selections, composed modules, hot reload, generated
  name collisions, and Metal's reflected no-op/folded-stage shapes.
- [x] (2026-09-14 19:37Z) Reconfigured and rebuilt against exact Slang commit
  `cdecb75031c1ce125985e51032c00a11c1f85492`; generated the stub, passed pre-commit and pyright,
  passed 121 focused native assertions and 19 Python configuration tests, and passed both legacy
  and repeated-record structural Vulkan runtime canaries.
- [x] (2026-09-14 19:39Z) Reproduced the CUDA dispatch crash in both the structural canary and the
  unchanged legacy canary (exit 139), establishing it as an inherited CUDA/OptiX test-host or
  backend baseline issue rather than a schema-only bridge regression.
- [x] (2026-09-14 19:58Z) Committed the validated bridge as
  `93b0e98b0e20f913e381d9b7a6809f95634b7236` and pushed
  `codex/dynamic-schema-host-bridge` to the user's SlangPy fork. This documentation closeout is a
  follow-up on the same branch; the outer Falcor gitlink will pin its resulting commit.
- [x] (2026-09-14 20:23Z) Corrected the native bridge regression to expect zero native
  payload/attribute pipeline sizes on Metal, as required by the reflection contract. The focused
  Vulkan bridge still passes 121/121 assertions and the full pre-commit suite passes; the Metal
  worker rerun is the remaining confirmation for this follow-up.

## Surprises and Discoveries

- Observation: the previous bridge models `ITraceProgramLayout`, whose reflected shader groups own
  static slot integers, and rejects every non-void record type.
  Evidence: `src/sgl/device/reflection.h`, `src/sgl/device/raytracing.cpp`, and
  `slangpy/core/calldata.py` expose `trace_program_layout` plus minimum table counts.

- Observation: function index is not a physical SBT index in the revised API. Its numbering is
  dense inside each payload partition for hit and miss functions, so two payloads may both contain
  function index zero.
  Evidence: the revised Slang reflection API exposes payload partitions and per-function indices;
  the host must therefore select records by stable source type identity, never by a schema-wide
  numeric lookup.

- Observation: the revised compiler changed the fourth `matrix` generic parameter from `int` to
  `MatrixLayoutMode`; SlangPy's static-array and printable-matrix extensions needed the same
  constraint before generated call shaders and Falcor shader imports could load.
  Evidence: the first runtime attempts failed in `slangpy/slang/staticarray.slang` and
  `src/sgl/device/print.slang`; after changing both constraints, the Vulkan canaries passed.

- Observation: source-level `NoClosestHit` is absent from logical stage reflection, but Metal may
  provide a group-level synthesized no-op closest-hit name for a dense visible function table.
  Evidence: `TraceProgramHitGroupInfo::closest_hit_entry_point_name` is now forwarded into the
  native hit-group descriptor even when `closest_hit` is absent, with focused regression coverage.

- Observation: revised Slang commit `cdecb75031c1ce125985e51032c00a11c1f85492` includes the
  OptiX register-count correction first landed in `f0ae84e7f330a3436aa7e38a26b3e67eda91569b`
  and scopes descriptor-storage erasure to D3D so Vulkan and CUDA keep source descriptor lowering.
  Evidence: final validation is pinned to `cdecb75031c1ce125985e51032c00a11c1f85492`; the
  compiler and downstream consumers are rebuilt after changing the pin so generated version and
  runtime binaries cannot silently remain at the earlier revision.

- Observation: the local CUDA runtime canary cannot currently provide comparative bridge evidence
  because it segfaults at dispatch in both legacy and structural modes.
  Evidence: on the exact compiler pin, `test_raytracing[DeviceType.cuda]` and
  `test_structural_raytracing[DeviceType.cuda]` both terminate with signal 11 at their respective
  dispatch calls, while both Vulkan variants pass.

- Observation: Metal correctly reflects zero for the native pipeline payload-size and hit-attribute
  settings because those host pipeline settings do not exist on Metal. The first cross-platform
  worker exposed that the native bridge test had incorrectly assumed portable-target values 4 and
  8 on every backend; the implementation itself returned the specified Metal values.
  Evidence: the Apple M4 worker built Slang and SGL, then reported only the two expectation
  mismatches in the 123-assertion bridge test.

## Decision Log

- Decision: identify host selections with reflected source type full names and translate them to
  compiler-generated native entry-point names only after the generated raygen and user module are
  composed.
  Rationale: source type names are stable at the public API boundary, whereas generated native
  names are a compiler implementation detail. Composition is required before materialization can
  see every imported or generated declaration.
  Date/Author: 2026-09-14 / Codex.

- Decision: retain legacy manually named ray-tracing arguments as a separate mode and make schema
  arguments mutually exclusive with them.
  Rationale: this migration must not regress existing SlangPy users, and silently combining both
  ownership models would make pipeline and SBT identity ambiguous.
  Date/Author: 2026-09-14 / Codex.

- Decision: permit repeated physical selections of one schema function and carry independent bytes
  for each record.
  Rationale: duplicated records and application data are core capabilities of a host-owned SBT;
  compacting selections by function would change runtime indexing semantics.
  Date/Author: 2026-09-14 / Codex.

- Decision: accept empty logical any-hit or intersection target names only for Metal reflection,
  where the compiler folds those stages into candidate dispatchers; diagnose them on portable
  targets.
  Rationale: Metal's finalized schema intentionally clears those per-stage target names, but
  silently dropping a portable stage would create an invalid native pipeline.
  Date/Author: 2026-09-14 / Codex.

- Decision: test native payload and attribute maxima as zero on Metal and retain the reflected 4/8
  expectations on targets with native pipeline size settings.
  Rationale: these values are target pipeline requirements, not ordinary source type sizes; making
  up nonzero Metal values would contradict the proposal and mislead hosts.
  Date/Author: 2026-09-14 / Codex.

## Outcomes and Retrospective

The bridge now reflects a finalized schema and accepts ordered physical hit, miss, and callable
record selections plus their application bytes. It preserves duplicates and sparse records,
materializes closed-schema entries regardless of `is_linked` provenance, resolves linked entries
from open schemas, and derives native payload and hit-attribute limits from reflection. Validation
on the exact compiler pin passes 121 native assertions, 19 configuration tests, Vulkan legacy and
structural runtime canaries, pre-commit, stub generation, and pyright. CUDA remains unclassified at
the backend level because both the pre-existing legacy canary and the new structural canary crash
at dispatch on this host; no structural-only CUDA regression has been observed.

## Context and Orientation

This repository is the `external/slangpy` submodule inside Falcor. Native SGL device reflection and
ray-tracing wrappers live in `src/sgl/device`. Nanobind declarations live in
`src/slangpy_ext/device`. The high-level functional API builds a generated `raygen_main` in
`slangpy/core/calldata.py` and exposes configuration through `slangpy/core/function.py` and
`slangpy/__init__.pyi`. Native tests live in `tests/sgl/device`; Python tests live under
`slangpy/tests`.

An SBT is the runtime array mapping numeric ray-tracing indices to miss, hit-group, and callable
shader records. A schema is a shader type implementing Slang's revised `ITraceProgramSchema`; it
declares legal stage implementations grouped by payload type, but it deliberately does not declare
physical SBT slots. A payload partition groups hit and miss implementations that use one ray payload
type. A function index identifies compiled code within a partition. It does not select a physical
record. Application record bytes are host data appended to a shader identifier in an SBT record.

The compiler dependency is the revised structural Slang implementation at commit
`cdecb75031c1ce125985e51032c00a11c1f85492`. Its nested RHI is commit
`5661193d9415fb3c84c068afb149b85ea7fe2310`, which includes application record data support. The
SlangPy nested RHI must use that exact commit so headers and runtime behavior agree.

## Plan of Work

First, update the nested RHI URL and gitlink to the fork commit used by the revised compiler. Inspect
its `ShaderTableDesc` and `ShaderRecordData` contract, then extend SGL's owning shader-table wrapper
so each physical record can keep independent bytes alive while the RHI creates its native table.

Second, replace the old `TraceProgramLayoutInfo` graph with a value-owning
`TraceProgramSchemaInfo`. Copy the schema's trace context, payload partitions, hit groups, miss
shaders, callables, source types, generated target names, record type/layout, linkage state, native
payload sizes, and maximum hit-attribute size. Reflection objects borrowed from Slang must not
outlive the program layout unless the wrapper retains the owning layout.

Third, rewrite `create_structural_ray_tracing_bindings` to accept physical source-type selections
for hit, miss, and callable records plus parallel record-byte arrays. Validate that every selection
is a member of the reflected schema and that byte sizes match the reflected record layout.
Materialize each unique synthesized stage once from the composed module, generate collision-free
native hit-group names, and preserve selection order and duplicates in the returned SBT arrays.
Lookups use full source type name plus stage kind; function indices are retained only as diagnostic
metadata. `is_linked` is reflection provenance, not selection eligibility: closed-schema catalogue
entries can report false and are intentionally materialized on demand.

Fourth, expose the new bridge through nanobind and Python. `FunctionNode.ray_tracing` accepts a
`trace_program_schema` string and physical selection arrays. Its configuration rejects mixed legacy
and schema modes and mismatched record-data lengths. `CallData` composes the generated raygen with
the user module, reflects and materializes the schema, derives payload and attribute limits, links
the pipeline, and creates the physical table. Cache keys include the schema name, ordered type
selections, ordered record bytes, recursion depth, and pipeline flags.

Finally, add tests. Native reflection tests cover two payload partitions, overlapping function
indices, repeated physical records, invalid type selections, ownership, and record data. Python
configuration tests cover mutual exclusion and cache identity. The runtime canary traces one hit
and one miss through the revised schema while SlangPy still generates raygen. All native build and
test commands run with an explicit maximum of eight jobs and through the Linux descendant-process
limiter.

## Concrete Steps

Run all commands below from
`/home/zhangkai/Documents/slangwork/slang-core-ecosys/falcor2/external/slangpy`. Native configure,
build, and test commands use the required limiter:

    export BUILD_LIMITER=/home/zhangkai/.codex/skills/limit-cpp-build-parallelism/scripts/run-limited-build.sh
    export REVISED_SLANG=/home/zhangkai/Documents/slangwork/slang-core-ecosys/another-slang-rt-integration
    test "$(git -C "$REVISED_SLANG" rev-parse HEAD)" = cdecb75031c1ce125985e51032c00a11c1f85492
    test "$(git -C external/slang-rhi rev-parse HEAD)" = 5661193d9415fb3c84c068afb149b85ea7fe2310
    CMAKE_BUILD_PARALLEL_LEVEL=8 MAX_JOBS=8 "$BUILD_LIMITER" \
      cmake --preset linux-gcc -S . --fresh -DSGL_LOCAL_SLANG=ON \
      -DSGL_LOCAL_SLANG_DIR:PATH="$REVISED_SLANG" \
      -DSGL_LOCAL_SLANG_BUILD_DIR=build/Release -DSGL_BUILD_TESTS=ON
    CMAKE_BUILD_PARALLEL_LEVEL=8 MAX_JOBS=8 "$BUILD_LIMITER" \
      cmake --build build/linux-gcc --config Debug \
      --target slangpy_ext sgl_tests slangpy_stub --parallel 8

After the build succeeds, run focused checks under the same process-tree cap:

    timeout 300s "$BUILD_LIMITER" build/linux-gcc/Debug/sgl_tests \
      --test-case='structural ray tracing native bridge' --no-colors=true
    PYTHONPATH="$PWD" "$BUILD_LIMITER" .venv/bin/python -m pytest \
      slangpy/tests/slangpy_tests/test_raytracing_config.py -v --device-types nodevice
    PYTHONPATH="$PWD" "$BUILD_LIMITER" .venv/bin/python -m pytest \
      slangpy/tests/slangpy_tests/test_raytracing.py -v --device-types vulkan,cuda
    .venv/bin/pre-commit run --all-files
    .venv/bin/pyright
    git diff --check

The focused native test must pass without treating function index as a unique physical slot. The
configuration suite must reject mixed modes and distinguish reordered or byte-different records in
cache identity. Supported runtime backends must return the expected canary pixels; an unsupported
backend must skip with an explicit capability reason.

## Validation and Acceptance

The bridge is accepted when a caller can select a revised schema, list physical hit and miss records
by source type, repeat a selected shader with distinct valid record bytes, and dispatch through a
SlangPy-generated raygen. A two-payload fixture must prove that overlapping function indices do not
alias. Invalid schema members, incorrect record byte sizes, mismatched arrays,
and mixed legacy/schema configuration must produce actionable diagnostics. The legacy canary must
remain unchanged and passing.

The outer Falcor migration is accepted separately when its structural shaders implement
`ITraceProgramSchema`, its host code supplies explicit physical records, and the reference path
tracer uses one multi-payload schema for scatter and pipeline visibility. Metal remains compile-only
until SGL's Metal backend gains native pipeline ray-tracing dispatch; that is a runtime backend
limitation rather than a schema design gap.

## Idempotence and Recovery

The source changes, configure, build, and test commands are repeatable. Never delete shared build
directories. If the nested RHI is wrong, re-check out the exact recorded commit instead of copying
individual headers. If Slang headers and libraries disagree, stop and repair the compiler build;
do not work around an ABI mismatch. If a synthesized stage cannot be materialized after composition,
record the exact type and module graph here before considering a compiler API change.

## Artifacts and Notes

The outer Falcor repository maintains the cross-repository change ledger in
`reports/structural-rt-port-checklist.md` and the concise current plan in
`reports/structural-rt-port-plan.md`. Exact test commands, commit pairs, backend outcomes, and known
limitations belong in that ledger after validation.

The historical branch-creation baselines (not the final dependency pins) are:

    Falcor:  b151beb
    SlangPy: 77205c2f3a5313c772d2df6c3cd19600887e938d
    Slang:   dcec1f948bd9162f3e402e15b3927f3681920507
    RHI:     5661193d9415fb3c84c068afb149b85ea7fe2310

Final validation instead pins Slang to `cdecb75031c1ce125985e51032c00a11c1f85492` and retains RHI
`5661193d9415fb3c84c068afb149b85ea7fe2310`.

## Interfaces and Dependencies

At completion, native SGL provides `ProgramLayout::find_trace_program_schema(name)`, value-owning
`TraceProgramSchemaInfo` metadata, and `create_structural_ray_tracing_bindings` accepting a schema
plus ordered physical hit, miss, and callable selections and application bytes. Its `ShaderTable`
wrapper owns and forwards record bytes through the revised RHI `ShaderRecordData` fields.

The Python API provides `FunctionNode.ray_tracing(trace_program_schema=...,
structural_hit_group_types=..., structural_miss_shader_types=...,
structural_callable_shader_types=..., structural_hit_group_record_data=...,
structural_miss_shader_record_data=..., structural_callable_shader_record_data=...)`. Legacy
`hit_groups`, entry-point arrays, and name arrays remain supported only when no schema is selected.

Revision note, 2026-09-14: created this living plan before implementation to capture the revised
ownership model, exact dependency identities, multi-payload invariant, interfaces, and validation
gates.
