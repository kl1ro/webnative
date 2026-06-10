import { makeApiRequest } from "../index.js";

let authPromise: Promise<Authentication> | undefined;

export function authenticate(): Promise<Authentication> {
  if (authPromise != undefined) return authPromise;
  authPromise = makeApiRequest<Authentication>("authenticate");
  return authPromise;
}

interface Authentication {
  port: number;
  key: string;
}
