import { MurmurData, MurmurOutputType } from '.';

export interface IHasher {
    update(message: MurmurData): IHasher;
    digest(type: MurmurOutputType): IHasher;
    // serialize(): IHasher; // TODO: clarify interface
    // copy(): IHasher; // TODO: clarify interface
}

export type HashType = 'murmurhash' | 'murmurhash128x64' |
    'murmurhash128' | 'murmurhash128x86';

export interface CreateHash {
    (type: HashType): IHasher;
}

export const createHash: CreateHash;
