import fs from 'fs';
import path from 'path';
import { DevSetupHelper, HOME_DIR } from './DevSetupHelper';
import { ANDROID_LINUX_COMMANDLINE_TOOLS } from './versions';
import { wrapInColor } from '../utils/logUtils';
import { ANSI_COLORS } from '../core/constants';
import { ubuntuSetup } from './distros/ubuntuSetup';
import { fedoraSetup } from './distros/fedoraSetup';

const BAZELISK_URL = 'https://github.com/bazelbuild/bazelisk/releases/download/v1.26.0/bazelisk-linux-amd64';

export const isFedora = fs.existsSync('/etc/fedora-release') || fs.existsSync('/etc/redhat-release');
export const isUbuntu = fs.existsSync('/etc/lsb-release') || fs.existsSync('/etc/debian_version');

export async function linuxSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();

  if (isUbuntu) ubuntuSetup(devSetup)
  else if (isFedora) fedoraSetup(devSetup)
  else console.log(wrapInColor('Your distro is not supported yet...aborting...', ANSI_COLORS.RED_COLOR));

  const bazeliskPathSuffix = '.valdi/bin/bazelisk';
  const bazeliskTargetPath = path.join(HOME_DIR, bazeliskPathSuffix);
  await devSetup.downloadToPath(BAZELISK_URL, bazeliskTargetPath);

  // Add executable permission to the downloaded binary
  const stats = fs.statSync(bazeliskTargetPath);
  fs.chmodSync(bazeliskTargetPath, stats.mode | 0o111);

  await devSetup.writeEnvVariablesToRcFile([{ name: 'PATH', value: `"$HOME/.valdi/bin:$PATH"` }]);

  await devSetup.setupAndroidSDK(ANDROID_LINUX_COMMANDLINE_TOOLS);

  devSetup.onComplete();
}