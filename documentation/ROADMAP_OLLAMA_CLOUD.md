# Roadmap: optional Ollama Cloud inference

Date: 20 November 2026. Status: **proposed, not implemented**.

## 1. Goal and scope

Keep the existing local game and add a startup choice between **Local** and
**Ollama Cloud**. Cloud mode is an optional way to test latency, model behavior,
and narrative quality. Local remains the default and works offline after the
existing installation and model download.

Both modes use the same game engine, persistent world, object registry, scene
validation, renderer, and audio system. The LLM continues to interpret natural
language; C++ validates and executes structured proposals. Hybrid scene
generation remains unchanged in principle: model proposals plus engine checks
and rendering. No C++ natural-language parser is introduced.

This roadmap is separate from the
[game-master dialogue roadmap](ROADMAP_DIALOGUE_MAITRE_DU_JEU.md). Adding cloud
access does not itself fix repetitive narration or unnecessary rendering.
The two efforts should share inference and archival interfaces, while keeping
their implementation and evaluation results distinguishable.

## 2. Verified starting point and provider constraints

The repository currently uses `gemma-4-E4B-it-Q4_K_M.gguf`, pinned by revision
and SHA-256 in `scripts/models.json`, through embedded llama.cpp.
`src/llm_runtime.h` already centralizes messages, generation configuration,
completion results, streaming callbacks, and token counting. However,
`CountChatPromptTokens` currently loads/verifies the local model when needed.
Cloud mode therefore requires more than replacing the completion function.

External facts below were checked on 20 November 2026 and must be checked again
before implementation:

| Topic | Finding | Consequence |
|---|---|---|
| Model identity | The public catalog lists Gemma 4 E4B for local execution and Gemma 4 31B for cloud execution. An E4B cloud variant was not found. | Label the initial comparison **local E4B vs cloud 31B**. Do not claim identical weights or quantization. See the [model catalog](https://ollama.com/library/gemma4/tags). |
| Authentication | Direct HTTPS access uses an API key; no local Ollama server is required. | Read `OLLAMA_API_KEY` and call the hosted API directly. See [authentication](https://docs.ollama.com/api/authentication). |
| Structured output | The documentation currently states that Ollama Cloud does not support structured outputs. | Do not assume that the local grammar guarantee transfers to the cloud. See [structured outputs](https://docs.ollama.com/capabilities/structured-outputs). |

Use `gemma4:31b` as the candidate direct-API model identifier documented by
Ollama, then verify it in the initial compatibility probe. Do not infer hosted
availability merely by appending `:cloud` to a local model tag. Record the
requested identifier and any identity returned by the provider. If an immutable
hosted revision is unavailable, record that limitation rather than inventing one.

Exactly matching the local E4B artifact remains a separate requirement: revisit
provider availability or evaluate hosting that artifact if this becomes essential.
It is not a prerequisite for the explicitly labeled 31B comparison proposed here.

## 3. Startup experience

After language selection, offer two choices in the selected language:

- **Local — Gemma 4 E4B:** runs on this computer; no network needed to play.
- **Ollama Cloud — Gemma 4 31B:** sends game prompts and relevant context online;
  requires a configured API key and incurs API usage charges.

Local is selected by default. Merely opening the menu must not contact the
provider or initialize a cloud request. Selecting Local must not require a key.
Selecting Cloud checks configuration and starts the cloud session only after
the player confirms that choice through the normal menu action.

When a key is missing or authentication fails, show a concise explanation and
allow return to the selector. Never display the key. Keep the startup screen
focused on playing; developer diagnostics contain schemas, transport details,
and capability reports.

The backend is fixed for the lifetime of a game session. No mid-turn switching,
automatic local-to-cloud fallback, or silent model substitution. A cloud failure
leaves the current world intact and permits a controlled retry. Starting a new
session allows a different backend choice.

Add equivalent explicit options for headless diagnostics and automated SDL
tests. Suggested option names are `--llm-backend local|ollama-cloud` and
`--cloud-model`; these are proposals, not existing commands. Explicit diagnostic
selection may bypass the menu and must still record the selected backend.

## 4. Implementation design

### 4.1. Shared inference boundary

Introduce a small backend dispatch behind the existing completion interface.
Keep the local llama.cpp implementation and its grammar-based generation intact.
The cloud adapter handles HTTPS, authentication, streamed responses, and mapping
provider results into the shared result type. The game runner must not contain
HTTP requests or provider-specific response parsing.

Audit every inference purpose: player interpretation, narration, scene
composition, repairs, localization, and thermal decisions. All calls in a cloud
session must use the chosen backend. A token-count operation must not silently
trigger local model loading. Exact inventory shortcuts remain inference-free.

Represent backend capabilities explicitly: constrained schema support, supported
sampling options, token-count precision, and model metadata availability.
Probe temperature, output limits, seed handling, and thinking controls before
claiming parity. Unsupported options must be visible in diagnostics; never
claim deterministic replay merely because a seed was sent.

For Windows, evaluate the system HTTPS stack, such as WinHTTP, to minimize new
runtime dependencies. Keep transport code isolated from other platforms and
review CMake linkage. Preserve certificate verification and keep credentials
scoped to the configured Ollama HTTPS origin, including redirect handling.

### 4.2. Structured proposals without cloud grammar guarantees

Use the same logical schemas and authoritative checks for both backends.
Cloud prompts describe the expected schema, but prompting is not enforcement.
The engine must check required fields, types, enums, bounds, extra fields,
identities, revisions, and physical preconditions before accepting a proposal.

For the initial cloud adapter:

1. Parse the complete final response and validate its schema and semantics.
2. On invalid structured output, permit at most one targeted repair request,
   within the interaction's overall time and output budgets.
3. If validation still fails, retain the previous world and show the appropriate
   localized failure or established safe fallback. Do not execute partial JSON.

Keep structural repair separate from the scene generator's existing bounded
composition attempts. Define a total interaction budget so these retry loops
cannot multiply without limit. Record every attempt and rejection reason.
Do not relax local validation to make cloud responses pass.

Tool calling may be evaluated during the compatibility probe, but it is not
assumed to provide the same schema guarantees as constrained decoding.
If reliability is insufficient, leave cloud mode experimental rather than
advertising it as equivalent to the local runtime.

### 4.3. Context and memory budgets

Preserve the full world in engine memory and continue selecting a bounded prompt
for each request. Apply the same conversation and continuity policy to both
backends, with explicit backend-specific accounting.

Local counting remains exact through the pinned tokenizer and template. For
cloud, first investigate whether a compatible tokenizer/template or a provider
counting facility is available. Do not assume the E4B count exactly matches the
hosted 31B request. If only an estimate is available, label it as estimated, use
a conservative tested input cap and output reserve, and handle context errors
without changing the world. Never report an estimate as a guaranteed token fit.

Cloud inference must not require loading the 5.34 GB local model merely to
budget a prompt. The existing installer may continue downloading the local
model in this first release; an optional-download installer is a separate scope.
Test cloud runtime startup with the local model path unavailable to expose
accidental dependencies.

### 4.4. Streaming, cancellation, and failure

Run network work outside the SDL event/audio loop. Parse streamed records across
arbitrary network chunk boundaries, including split UTF-8 sequences. Bound body
size, output length, connection/read timeouts, and total interaction duration.
Map provider completion reasons explicitly; truncated output is not success.

Cancellation or window closure must stop local processing promptly and discard
unpublished candidates. A provider may already have processed or billed the
request; do not promise remote cancellation or zero cost after local cancellation.
Handle authentication errors, rate limits, service failures, interrupted streams,
and malformed responses without corrupting state or freezing the UI.

Do not automatically resend a request of uncertain completion indefinitely.
Any permitted transport retry has a bounded policy and a distinct attempt ID.
HTTP success alone never authorizes world publication. Continue to publish state,
scene layers, inventory, and audio coherently after validation, and keep
provisional physical outcomes out of player-visible streaming text.

## 5. Credentials and research archives

Read the API key from `OLLAMA_API_KEY` in the launched process environment.
Do not put it in command-line arguments, tracked configuration, screenshots,
request dumps, exception messages, or distributed binaries. Each tester supplies
their own key for this first implementation. A shared paid service for public
players would require a separate server/account design.

Extend the existing archives and align them with the complete-interaction
archives in the dialogue roadmap. Record:

- session backend, requested/reported model, build and prompt/schema versions;
- per-call role, interaction/attempt IDs, exact message payload and non-secret
  generation settings, including settings rejected or unsupported by the provider;
- raw available responses, parsed proposals, validation/repair decisions, displayed
  text, completion reason, durations, and reported usage;
- state before/after, accepted and rejected scene artifacts, audio decisions and
  effective settings, and a full SDL screenshot after each completed interaction;
- explicit missing/incomplete evidence for interrupted or failed interactions.

For local inference, retain the exact formatted prompt. For hosted inference,
archive the exact outbound messages and payload; the server's final templated
prompt may not be exposed. Mark it unavailable instead of fabricating an exact
prompt or claiming an identical template. Apply the same rule to unavailable
model revisions, sampling internals, and token usage on interrupted calls.

Unchanged scene assets may be referenced by verified immutable hashes; the
new interaction still needs its own presentation capture. Capturing SDL does
not require generating a new scene or raytracing again. Archives remain outside
Git and never become automatic inputs to a new game.

## 6. Cost and effort

The published Gemma 4 rate on 20 November 2026 is $0.14 per million input tokens,
$0.05 per million cached input tokens, and $0.40 per million output tokens.
Pay-as-you-go is available without a subscription. Recheck rates before a live
benchmark. See [Ollama pricing](https://ollama.com/pricing).

For a deliberately illustrative workload totaling 10,000 uncached input tokens
and 1,000 output tokens across all calls in one interaction:

```text
estimated interaction cost = 10,000 / 1,000,000 * $0.14
                           +  1,000 / 1,000,000 * $0.40
                           = $0.0018
100 such interactions     = $0.18
```

This is not a measured game-session price. Scene generation, context size,
repairs, and interrupted requests affect usage. Count all calls, avoid assuming
cache discounts, and distinguish reported usage from estimates. A local cost
estimate is not the provider's billing ledger or a guaranteed spending cap.
Automated cloud recipes must use a finite call/token budget and stop on repeated
failures instead of spending through an unbounded retry loop.

Initial engineering estimate: several developer-days for a usable, tested
integration. The main uncertainties are schema reliability, context accounting,
stream cancellation, and regression coverage. Refine this estimate after the
first real API probe; it is not a delivery commitment. No API key or paid call
is required to prepare this roadmap.

## 7. Delivery stages

| Stage | Deliverable | Exit criteria |
|---|---|---|
| CLOUD-00 — compatibility probe | Bounded live tests of model access, JSON compliance, streaming, sampling options and usage metadata. | Record actual capabilities and errors; establish whether a full gameplay trial is viable. No gameplay changes yet. |
| CLOUD-01 — runtime boundary | Backend selection/configuration and isolated HTTPS adapter in the inference layer; CMake wiring. | Local regression tests pass; mock transport covers stream/error mapping; cloud path does not load local weights. |
| CLOUD-02 — contracts and budgets | Strict cloud proposal validation, one repair maximum, overall retry/time limits, context accounting. | Invalid, stale or truncated proposals cannot mutate state; all LLM roles use the selected backend. |
| CLOUD-03 — startup selector | Local/default and cloud choices, missing-key/error handling, diagnostic selection and localized UI. | Local starts offline without a key; cloud choice works with configured credentials; no silent fallback. |
| CLOUD-04 — evidence and cost | Correlated calls, scene/state/audio records, full captures and usage estimates. | Each tested interaction can be inspected end to end; credentials never appear in evidence. |
| CLOUD-05 — autonomous comparison | Local/cloud gameplay recipes, latency and quality report, corrected regressions. | The acceptance criteria below pass, with observed limitations documented before release. |

Start minimal call tracing in CLOUD-00; CLOUD-04 completes the unified archive.
Codex owns interfaces, semantics, memory, builds, review and integration.
Delegate bounded tests or documentation through the repository's contracted
Copilot/Ollama workflow; update `documentation/duo/PROGRESS.md` after each batch.
The development duo's DeepSeek configuration does not select the game's model.

## 8. Autonomous validation and acceptance

Codex runs and evaluates the implementation rather than leaving qualification to
the next player. Combine deterministic tests, actual local/cloud inference, and
SDL sessions. A cloud key unavailable during implementation is a recorded test
blocker, not a reason to report simulated cloud success.

| Area | Required checks |
|---|---|
| Local preservation | Start and play without network or key; pinned model and grammar remain active; existing tests pass. |
| Startup | Both choices, six languages, missing/invalid key, cloud unavailable, exit/back navigation, explicit headless mode. |
| Transport | Split JSON/UTF-8 chunks, empty or truncated stream, oversized response, timeout, 401/403, 429, 5xx and cancellation. |
| Authority | Invalid schema, stale revision, unknown object, impossible move, source already consumed; no unintended mutation or duplication. |
| Gameplay | New room, revisited room, blocked exit, first invisible-wall contact, take/drop, synonyms and pronouns, inventory, clues and audio. |
| Evidence | Call-to-interaction linkage, every attempt retained, captures match published state, usage uncertainties explicit, no key leakage. |
| Resources | No unnecessary local GGUF load in cloud mode; responsive SDL/audio; bounded requests, buffers and shutdown. |

Compare identical build versions, initial fixtures, commands, prompt versions,
and application output/context budgets. Use independent sessions per backend.
Generated worlds will diverge: combine controlled headless fixtures for matched
tasks with full SDL journeys for experiential review. SDL does not gain archive
loading as part of this work.

Record time to first displayed text, total interaction time, generation versus
render time, tokens, repair/fallback rate, failures and estimated cost. Separate
local cold model loading from warm runs, and use at least 30 comparable text
interactions per backend when reporting median/p95 latency. Repeat selected
scenarios across at least three seeds/runs; a remote seed is not proof of
determinism. Do not attribute quality differences solely to hosting when the
comparison is E4B versus 31B.

Review actual prose and screenshots for relevance, coherence, naturalness,
repetition, object identity and factual consistency. Reuse the dialogue roadmap's
scoring rubric where applicable, but only require its new conversation behavior
after that feature is implemented. Preserve failed examples and rerun fixes.

Release gate: zero observed authoritative-state violations; local/offline
regressions resolved; expected cloud failures recover cleanly; all completed
SDL test interactions have their required evidence. Set a reliability threshold
after CLOUD-00, before the final benchmark. Keep cloud mode labeled experimental
if proposal reliability or latency fails that threshold. Publish measured results
and unresolved limitations, not claims of universal model parity.

## 9. Current delivery status

This change delivers documentation only. No cloud backend, startup selector,
credential storage, installer change or live API test has been implemented.
The current local runtime remains the reference implementation.
