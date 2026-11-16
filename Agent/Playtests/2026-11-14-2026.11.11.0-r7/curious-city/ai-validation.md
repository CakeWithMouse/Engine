# AI validation and runtime scenario (own r7 run)

Request/result/log: `Engine/build/playtests/20261114-r7/curious-city/requests/ai-validation.request.json`, `results/ai-validation.result.json`, and `logs/ai-validation.log`.

With `CityGuard` assigned to `/Game/AI/CityGate2.fsm`, I changed node 2 to task `Ai.Fly`, saved/checked it, and tried Play. Check returned the node-keyed diagnostic for node `2` (`task 1: unknown block Ai.Fly`); Play returned control status 6 and remained Edit (playState 0). I then repaired the same node to `Ai.Wait` with seconds 0.15. The next check cleared diagnostics; Play returned status 0 and playState 1. After 40 frames at 0.05 s, the AI extension reported both CityGuard and its duplicate as running in `Idle`, with memory Speed 1.75 and 5.5 respectively and a start/completion history. Stop returned to Edit.

Boundary: this proves the real check/Play refusal/fix/Play path and runtime API state. The AI card image (`frames/ai-valid-live.png`) was captured before the main-window one-second AI polling timer had refreshed, so its visible `Ai.Fly` text is stale and is not evidence of post-fix card UI. Recheck after a >1 s real UI pump and capture the live card through the coordinator's new scroll action. This graph's completed transition has a 0.25 s delay, and the observed live state stayed Idle; no transition result is claimed.
