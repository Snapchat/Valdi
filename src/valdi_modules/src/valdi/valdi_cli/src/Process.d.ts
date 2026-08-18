declare global {
  interface ProcessStdout {
    write(value: string): boolean;
  }

  interface Process {
    arch: string;
    argv?: string[];
    env: { [key: string]: string | undefined };
    exit?: (code?: number) => void;
    pid: number;
    stdout?: ProcessStdout;
    version: number;
  }

  const process: Process;
}

export {};
