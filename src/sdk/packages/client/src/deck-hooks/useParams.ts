import { ReactRouter } from '../webpack';

/**
 * Get the current params from ReactRouter
 *
 * @returns an object with the current ReactRouter params
 *
 * @example
 * import { useParams } from "decky-frontend-lib";
 *
 * const { appid } = useParams<{ appid: string }>()
 */
export const useParams = Object.values(ReactRouter).find((val) => /return (\w)\?\1\.params:{}/.test(`${val}`)) as <
  T,
>() => T;
