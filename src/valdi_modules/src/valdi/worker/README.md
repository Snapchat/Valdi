# How to run tests 

## For TypeScript only changes
See [testing with the hotreloader](../../../../../docs/docs/workflow-testing.md#how-to-iterate-on-my-tests-using-the-hot-reloader)

## For C++ changes you'd need to rebuild valdi_standalone 
1. Start hot reloader `valdi hotreload`
2. From the repo root run `bzl run valdi:valdi_standalone -- --log_level debug --hot_reload --script_path valdi_standalone/src/TestsRunner -- --include worker/test/WorkerTest
   `. Note the `--include` to pick specific tests to run. 