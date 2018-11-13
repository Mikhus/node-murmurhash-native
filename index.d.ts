export type MurmurReturnValue = number | Buffer | string | undefined;
export type MurmurEncoding = 'utf8' | 'ucs2' | 'ascii' | 'hex' | 'base64' |
    'binary';
export type MurmurOutputType = 'number' | 'hex' | 'base64' | 'binary' |
    'buffer';
export type MurmurData = string | Buffer;
export interface MurmurCallback {
    (err: Error, result?: MurmurReturnValue): any;
}
export interface MurmurFunction {
    (
        data: MurmurData,
        seed: number,
        output?: MurmurOutputType,
        offset?: number,
        length?: number,
        callback?: MurmurCallback,
    ): MurmurReturnValue;
    (
        data: MurmurData,
        output: Buffer,
        offset?: number,
        length?: number,
        callback?: MurmurCallback,
    ): Buffer;
    (
        data: MurmurData,
        output_type: MurmurOutputType,
        seed?: number,
        callback?: MurmurCallback,
    ): MurmurReturnValue;
    (
        data: string,
        encoding: MurmurEncoding,
        callback?: MurmurCallback,
    ): MurmurReturnValue;
    (
        data: string,
        output_type: MurmurOutputType,
        callback?: MurmurCallback,
    ): MurmurReturnValue;
    (data: MurmurData, callback?: MurmurCallback): MurmurReturnValue;
}

export declare const murmurHash: MurmurFunction;
export declare const murmurHash32: MurmurFunction;
export declare const murmurHash128: MurmurFunction;
export declare const murmurHash128x64: MurmurFunction;
export declare const murmurHash128x86: MurmurFunction;
export declare const murmurhash64: MurmurFunction;
export declare const murmurHash64x64: MurmurFunction;
export declare const murmurHash64x32: MurmurFunction;

export declare const BE = {
    murmurHash,
    murmurHash32,
    murmurHash128,
    murmurHash128x64,
    murmurHash128x86,
    murmurhash64,
    murmurHash64x64,
    murmurHash64x32,
};

export declare namespace BE {
    export const murmurHash: MurmurFunction;
    export const murmurHash32: MurmurFunction;
    export const murmurHash128: MurmurFunction;
    export const murmurHash128x64: MurmurFunction;
    export const murmurHash128x86: MurmurFunction;
    export const murmurhash64: MurmurFunction;
    export const murmurHash64x64: MurmurFunction;
    export const murmurHash64x32: MurmurFunction;
}
export declare namespace LE {
    export const murmurHash: MurmurFunction;
    export const murmurHash32: MurmurFunction;
    export const murmurHash128: MurmurFunction;
    export const murmurHash128x64: MurmurFunction;
    export const murmurHash128x86: MurmurFunction;
    export const murmurhash64: MurmurFunction;
    export const murmurHash64x64: MurmurFunction;
    export const murmurHash64x32: MurmurFunction;
}
export declare namespace platform {
    export const murmurHash: MurmurFunction;
    export const murmurHash32: MurmurFunction;
    export const murmurHash128: MurmurFunction;
    export const murmurHash128x64: MurmurFunction;
    export const murmurHash128x86: MurmurFunction;
    export const murmurhash64: MurmurFunction;
    export const murmurHash64x64: MurmurFunction;
    export const murmurHash64x32: MurmurFunction;
}
