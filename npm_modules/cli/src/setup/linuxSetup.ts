import fs from 'fs';
import path from 'path';
import { checkCommandExists } from '../utils/cliUtils';
import { DevSetupHelper, HOME_DIR } from './DevSetupHelper';

const BAZELISK_URL = 'https://github.com/bazelbuild/bazelisk/releases/download/v1.26.0/bazelisk-linux-amd64';

export async function linuxSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();

  await devSetup.runShell('Installing dependencies from apt', [
    `sudo apt-get install zlib1g-dev git-lfs watchman libfontconfig-dev adb`,
  ]);

  await devSetup.runShell('Installing libtinfo5', [
    `wget http://security.ubuntu.com/ubuntu/pool/universe/n/ncurses/libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
    `sudo apt install ./libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
  ]);

  if (!checkCommandExists('java')) {
    await devSetup.runShell('Installing Java Runtime Environment', ['sudo apt install default-jre']);
  }

  const bazeliskPathSuffix = '.valdi/bin/bazelisk';
  const bazeliskTargetPath = path.join(HOME_DIR, bazeliskPathSuffix);
  await devSetup.downloadToPath(BAZELISK_URL, bazeliskTargetPath);

  // Add executable permission to the downloaded binary
  const stats = fs.statSync(bazeliskTargetPath);
  fs.chmodSync(bazeliskTargetPath, stats.mode | 0o111);

  await devSetup.writeEnvVariablesToRcFile([{ name: 'PATH', value: `"$HOME/.valdi/bin:$PATH"` }]);

  await devSetup.setupAndroidSDK();

  devSetup.onComplete();
}
