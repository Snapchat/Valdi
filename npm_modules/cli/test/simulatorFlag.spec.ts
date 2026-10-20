import yargs, { type Argv } from 'yargs';
import * as buildCommand from '../src/commands/build';
import * as installCommand from '../src/commands/install';
import { IOS_DEVICE_BUILD_FLAGS, PLATFORM } from '../src/core/constants';
import { resolveBazelBuildArgs } from '../src/utils/applicationUtils';
import { isDeviceBuild } from '../src/utils/buildInfo';

type CommandModule = { builder: (yargs: Argv<any>) => void };

function iosBazelArgs(command: CommandModule, flags: string[]): string {
  const argv = yargs(['ios', ...flags]) as Argv<any>;
  command.builder(argv);
  const simulator = argv.parseSync()['simulator'] as boolean | undefined;
  return resolveBazelBuildArgs(PLATFORM.IOS, 'debug', [], isDeviceBuild(simulator), false, false, '');
}

for (const [name, command] of [
  ['install', installCommand],
  ['build', buildCommand],
] as const) {
  describe(`valdi ${name} ios`, () => {
    it('builds for the simulator by default', () => {
      expect(iosBazelArgs(command, [])).not.toContain(IOS_DEVICE_BUILD_FLAGS);
    });

    it('builds for the simulator with --simulator', () => {
      expect(iosBazelArgs(command, ['--simulator'])).not.toContain(IOS_DEVICE_BUILD_FLAGS);
    });

    it('builds for a device with --no-simulator', () => {
      expect(iosBazelArgs(command, ['--no-simulator'])).toContain(IOS_DEVICE_BUILD_FLAGS);
    });
  });
}
