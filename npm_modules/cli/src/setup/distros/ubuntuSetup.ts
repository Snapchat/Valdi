import { checkCommandExists } from "../../utils/cliUtils";
import { DevSetupHelper } from "../DevSetupHelper";

export async function ubuntuSetup(devSetup: DevSetupHelper): Promise<void> {
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
}