# LLM API Guide (Draft)

## Endpoint

- OpenAI-compatible `/v1/chat/completions`

## Workflow

1. Send model context snapshot.
2. Receive PR-style proposal (diff + commit metadata).
3. Commit proposal to local feature branch.
4. Run checker.
5. Await user merge decision.
