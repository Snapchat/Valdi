import { ClientSQLNativeModule, openDatabase } from './ClientSQLNative';

export const clientSQLNative: ClientSQLNativeModule = {
  openDatabase: openDatabase as unknown as ClientSQLNativeModule['openDatabase'],
};
