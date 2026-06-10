import net from "net";

export function createPipe() {
  const pipeName = process.argv[2];

  return new Promise((resolve) => {
    const socket = net.createConnection(pipeName, () => resolve(socket));
  });
}
