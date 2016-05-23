#include "static_assert.h"
#include "hasher.h"
#include "inputdata.h"
#include "murmurhashutils.h"
#include "hasher_impl.h"
#include "asyncupdate.h"

namespace MurmurHash {
  using v8::Object;
  using v8::Int32;
  using v8::Uint32;
  using v8::Function;
  using v8::ObjectTemplate;
  using v8::PropertyAttribute;
  using v8::ReadOnly;
  using v8::DontDelete;
  using v8::DontEnum;
  using Nan::MaybeLocal;

  #define SINGLE_ARG(...) __VA_ARGS__

  namespace {
    static const char * const MESSAGE_ERROR_PENDING_UPDATE = "Asynchronous update still in progress";
  }

  /**
   * @class
   *
   * Create MurmurHash utility
   * 
   * new MurmurHash([seed|hash|serial])
   *
   * @param {number} seed - initial murmur hash seed as 32 bit integer
   * @param {MurmurHash} hash - an instance of another MurmurHash of the same type
   * @param {string|Buffer} serial - serialized state of the same MurmurHash type
  **/
  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_METHOD(SINGLE_ARG(IncrementalHasher<H,HashValueType,HashLength>::New))
  {

    IncrementalHasher_T *self;

    if ( info.IsConstructCall() ) {

      if ( info.Length() > 0 ) {
        if ( info[0]->IsNumber() ) {
          // seed
          uint32_t seed = Nan::To<uint32_t>(info[0]).FromMaybe(0U);
          self = new IncrementalHasher_T(seed);

        } else if ( Nan::New(constructor)->HasInstance( info[0] ) ) {
          // hasher instance
          IncrementalHasher_T *other = ObjectWrap::Unwrap<IncrementalHasher_T>( info[0].As<Object>() );

          if ( other->asyncInProgress ) {
            return Nan::ThrowError(MESSAGE_ERROR_PENDING_UPDATE);
          }

          self = new IncrementalHasher_T(*other);

        } else if ( info[0]->IsString() ) {
          // serial string
          uint8_t serial[kHashSerialSize];
          if ( kHashSerialStringSize == info[0].As<String>()->Length() ) {
            Nan::DecodeWrite( (char *) serial, sizeof(serial), info[0], Nan::BASE64 );
          } else {
            return Nan::ThrowTypeError("Incorrect size of the serialized string");
          }
          if ( IsSerialTypeValid( serial ) ) {
            self = new IncrementalHasher_T(serial);
          } else {
            return Nan::ThrowTypeError("Incorrect serialized string");
          }

        } else if ( node::Buffer::HasInstance( info[0] ) ) {
          // serial buffer
          if ( kHashSerialSize <= static_cast<int32_t>(node::Buffer::Length( info[0] )) ) {
            uint8_t *serial = (uint8_t *) node::Buffer::Data( info[0] );
            if ( IsSerialTypeValid( serial ) ) {
              self = new IncrementalHasher_T(serial);
            } else {
              return Nan::ThrowTypeError("Incorrect serialized data");
            }
          } else {
            return Nan::ThrowTypeError("Incorrect size of the serialized data");
          }

        } else if ( info[0]->IsUndefined() || info[0]->IsNull() ) {
          self = new IncrementalHasher_T();
        } else {
          return Nan::ThrowTypeError("Expected a seed number, MurmurHash instance or serialized state");
        }

      } else {
        self = new IncrementalHasher_T();
      }

      self->Wrap( info.This() );
      info.GetReturnValue().Set( info.This() );

    } else {
      int argc = std::min(1, info.Length());
      Local<Value> argv[1];
      if (argc > 0) argv[0] = info[0];
      Local<Function> cons = Nan::GetFunction(Nan::New(constructor)).ToLocalChecked();
      MaybeLocal<Object> mayinst = Nan::NewInstance(cons, argc, &argv[0]);
      if ( ! mayinst.IsEmpty() ) {
        info.GetReturnValue().Set( mayinst.ToLocalChecked() );
      }
    }
  }

  /**
   * Copy the internal state onto the target utility instance
   * 
   * copy(target)
   * 
   * @param {MurmurHash} target - a different instance of MurmurHash utility
   *                              of the same type
   * @return {MurmurHash} target
  **/
  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_METHOD(SINGLE_ARG(IncrementalHasher<H,HashValueType,HashLength>::Copy))
  {
    IncrementalHasher_T *self = ObjectWrap::Unwrap<IncrementalHasher_T>( info.Holder() );

    if ( self->asyncInProgress ) {
      return Nan::ThrowError(MESSAGE_ERROR_PENDING_UPDATE);
    }

    if ( info.Length() > 0 && Nan::New(constructor)->HasInstance( info[0] ) ) {
      IncrementalHasher_T *other = ObjectWrap::Unwrap<IncrementalHasher_T>( info[0].As<Object>() );
      if ( other == self ) {
        return Nan::ThrowError("Target must not be the same instance");
      }
      *other = *self;
    } else {
      return Nan::ThrowTypeError("Target must be another instance of the same murmur hash type utility");
    }

    info.GetReturnValue().Set( info[0] );
  }

  /**
   * Generate the murmur hash of all of the data provided so far
   * 
   * digest([output_type])
   * digest(output, offset, length)
   * 
   * output_type indicates the form and encoding of the returned hash and can be one of:
   *
   * - 'number' - for 32-bit murmur hash an unsigned 32-bit integer,
   *              other hashes - hexadecimal string
   * - 'hex'    - hexadecimal string
   * - 'base64' - base64 string
   * - 'binary' - binary string
   * - 'buffer' - a new Buffer object
   *
   * If neither output nor known output_type is provided a Buffer is returned.
   * 
   * @param {string} [output_type]
   * @param {Buffer} output - a Buffer object to write hash bytes to;
   *       the same object will be returned
   * @param {number} offset - start writing into output at offset byte;
   *       negative offset starts from the end of the output buffer
   * @param {number} length - a number of bytes to write from calculated hash;
   *       negative length starts from the end of the hash;
   *       if absolute value of length is larger than the size of a calculated
   *       hash, bytes are written only up to the hash size
   * @return {Buffer|string|number} murmur hash
  **/
  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_METHOD(SINGLE_ARG(IncrementalHasher<H,HashValueType,HashLength>::Digest))
  {
    IncrementalHasher_T *self = ObjectWrap::Unwrap<IncrementalHasher_T>( info.Holder() );

    if ( self->asyncInProgress ) {
      return Nan::ThrowError(MESSAGE_ERROR_PENDING_UPDATE);
    }

    OutputType outputType( DefaultOutputType );

    int argc = info.Length();

    if ( argc > 0 ) {
      if ( info[0]->IsString() ) {
        InputData::ReadEncodingString( info[0].As<String>() );
        outputType = InputData::DetermineOutputType();
      } else if ( node::Buffer::HasInstance( info[0] ) ) {
        outputType = ProvidedBufferOutputType;
      }
    }

    Local<Value> result;

    HashValueType hash[HashLength];

    self->Digest( hash );

    switch(outputType) {
      case HexStringOutputType:
        result = HashToHexString<HashLength>( hash );
        break;

      case BinaryStringOutputType:
        result = HashToEncodedString<HashLength>( hash, Nan::BINARY );
        break;

      case Base64StringOutputType:
        result = HashToEncodedString<HashLength>( hash, Nan::BASE64 );
        break;

      case NumberOutputType:
        if (HashSize == sizeof(uint32_t)) {
          result = Nan::New<Uint32>( (uint32_t) (*hash) );
        } else {
          result = HashToHexString<HashLength>( hash );
        }
        break;

      case ProvidedBufferOutputType:
        result = info[0];
        WriteHashToBuffer<HashLength>(
              hash,
              node::Buffer::Data(result),
              (int32_t) node::Buffer::Length(result),
              (argc > 1) ? Nan::To<int32_t>(info[1]).FromMaybe(0) : 0,
              (argc > 2)
              ? Nan::To<int32_t>(info[2]).FromMaybe(HashSize)
              : HashSize);
        break;

      default:
        result = Nan::NewBuffer( HashSize ).ToLocalChecked();
        WriteHashBytes<HashLength>(hash, (uint8_t *) node::Buffer::Data(result));
        break;
    }

    info.GetReturnValue().Set( result );
  }

  /**
   * Serialize the internal state of the murmur hash utility instance
   * 
   * serialize([output[, offset]])
   * 
   * If the output buffer is not provided the serial is generated as a base64
   * encoded string. When output has not enough space for the serialized data
   * at the given offset it throws an Error. You may consult the required
   * byte length reading constant: MurmurHashClass.SERIAL_BYTE_LENGTH
   * 
   * @param {Buffer} output - a buffer to write serialized state to
   * @param {number} offset - offset at output
   * @return {string|Buffer}
  **/
  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_METHOD(SINGLE_ARG(IncrementalHasher<H,HashValueType,HashLength>::Serialize))
  {
    IncrementalHasher_T *self = ObjectWrap::Unwrap<IncrementalHasher_T>( info.Holder() );

    if ( self->asyncInProgress ) {
      return Nan::ThrowError(MESSAGE_ERROR_PENDING_UPDATE);
    }

    int argc = info.Length();

    Local<Value> result;

    if ( argc > 0 && node::Buffer::HasInstance( info[0] ) ) {

      int32_t length = static_cast<int32_t>(node::Buffer::Length( info[0] ));
      int32_t offset = (argc > 1) ? Nan::To<int32_t>(info[1]).FromMaybe(0) : 0;

      if (offset < 0) offset += length;

      if (offset >= 0 && kHashSerialSize <= length - offset ) {

        result = info[0];
        uint8_t *serial = (uint8_t *) node::Buffer::Data( result ) + offset;
        self->Serialize(serial);

      } else {
        return Nan::ThrowError("Serialized state does not fit in the provided buffer at the given offset");
      }
    } else {

      uint8_t serial[kHashSerialSize];
      self->Serialize(serial);
      result = Nan::Encode((void *)serial, sizeof(serial), Nan::BASE64);

    }

    info.GetReturnValue().Set( result );
  }

  /**
   * Update internal state with the given data
   *
   * update(data[, encoding][, callback])
   *
   * The encoding can be 'utf8', 'ascii', 'binary', 'ucs2', 'base64' or 'hex'.
   * If encoding is not provided or is not known and the data is a string,
   * an encoding of 'utf8' is enforced. If data is a Buffer then encoding is ignored.
   * 
   * @param {string|Buffer} data
   * @param {string} [encoding]
   * @param {Function} callback - optional callback(err)
   *       if provided the hash will be updated asynchronously using libuv
   *       worker queue, the return value in this instance will be `undefined`
   *       Be carefull as reading and writing by multiple threads to the same
   *       memory may render undetermined results
   * @return {MurmurHash} this
  **/
  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_METHOD(SINGLE_ARG(IncrementalHasher<H,HashValueType,HashLength>::Update))
  {
    IncrementalHasher_T *self = ObjectWrap::Unwrap<IncrementalHasher_T>( info.Holder() );

    InputData data;

    int argc = info.Length();

    if ( argc > 0 ) {
      enum Nan::Encoding encoding = Nan::BUFFER;
      int callbackIndex = -1;

      if ( argc > 1 && info[argc - 1]->IsFunction() ) {
        callbackIndex = --argc;
      }

      if ( info[0]->IsString() ) {
        encoding = Nan::UTF8;

        if ( argc > 1 && info[1]->IsString() ) {
          InputData::ReadEncodingString( info[1].As<String>() );
          (void) InputData::DetermineEncoding( encoding );
        }

      }

      if ( callbackIndex > -1 ) {

        IncrementalHashUpdater<H,HashValueType,HashLength> *asyncUpdater;
        Nan::Callback *callback = new Nan::Callback(
                                    Local<Function>::Cast(info[callbackIndex]));

        if ( self->asyncInProgress ) {
          Local<Value> argv[] = {
            v8::Exception::Error(Nan::New<String>(MESSAGE_ERROR_PENDING_UPDATE).ToLocalChecked())
          };
          callback->Call(1, argv);
          delete callback;

          return; // undefined by default

        } else if ( argc > 0 ) {
          asyncUpdater = new IncrementalHashUpdater<H,HashValueType,HashLength>(
                                                      callback, self, info[0], encoding);
        } else {
          asyncUpdater = new IncrementalHashUpdater<H,HashValueType,HashLength>(callback);
        }

        self->asyncInProgress = true;

        Nan::AsyncQueueWorker(asyncUpdater);

        return; // undefined by default

      } else if ( self->asyncInProgress ) {

        return Nan::ThrowError(MESSAGE_ERROR_PENDING_UPDATE);

      } else {

        data.Setup( info[0], encoding );

      }
    }

    if ( ! data.IsValid() )
      return Nan::ThrowTypeError(data.Error());

    self->Update( (const void *) *data, (int32_t) data.length());

    info.GetReturnValue().Set( self->handle() );
  }

  /**
   * @property {boolean} isBusy - is asynchronous update in progress
  **/
  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_GETTER(SINGLE_ARG(IncrementalHasher<H,HashValueType,HashLength>::GetIsBusy))
  {
    IncrementalHasher_T *self = ObjectWrap::Unwrap<IncrementalHasher_T>( info.Holder() );
    info.GetReturnValue().Set( Nan::New(self->asyncInProgress) );
  }

  /**
   * @property {number} total - (read only) The total (modulo 2^32) bytes of data
   *                                                              provided so far
  **/
  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_GETTER(SINGLE_ARG(IncrementalHasher<H,HashValueType,HashLength>::GetTotal))
  {
    IncrementalHasher_T *self = ObjectWrap::Unwrap<IncrementalHasher_T>( info.Holder() );
    info.GetReturnValue().Set( Nan::New<Uint32>(self->total) );
  }

  #undef SINGLE_ARG

  /*************************************************/
  /******************* internal ********************/
  /*************************************************/

  /*---------------- constructors -----------------*/

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE IncrementalHasher<H,HashValueType,HashLength>
  ::IncrementalHasher(const uint32_t seed) : hasher(seed), total(0), asyncInProgress(false) {};

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE IncrementalHasher<H,HashValueType,HashLength>
  ::IncrementalHasher(const IncrementalHasher_T& other) : ObjectWrap(),
                                                          hasher(other.hasher),
                                                          total(other.total),
                                                          asyncInProgress(false) {};

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE IncrementalHasher<H,HashValueType,HashLength>
  ::IncrementalHasher(const uint8_t *serial) : hasher(serial), asyncInProgress(false)
  {
    ReadHashBytes<1>(&serial[kHashSerialTotalIndex], &total);
  }

  /*--------------- static methods ----------------*/

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE bool IncrementalHasher<H,HashValueType,HashLength>
  ::IsSerialTypeValid(uint8_t *serial)
  {
    // check state type
    if (kHashSerialType == (serial[kHashSerialTypeIndex] & kHashSerialTypeMask)) {
      // read checksum
      checksum_t chksum = (checksum_t) serial[kHashSerialCkIndex];
      for(int i = kHashSerialCkIndex;
          ++i < kHashSerialSize;
          chksum = (chksum << 8) | serial[i]);
      // build verify
      checksum_t verify = PMurHash32(serial, kHashSerialSize - kHashSerialCkSize, kHashSerialCkSeed);
      STATIC_ASSERT(kHashSerialCkSize > 0 && kHashSerialCkSize <= sizeof(checksum_t),
                    "must have 1 <= kHashSerialCkSize <= sizeof(checksum_t)");
      if (kHashSerialCkSize < sizeof(checksum_t)) {
        chksum ^= (verify >> ((sizeof(checksum_t) - kHashSerialCkSize)*8));
      }
      // verify checksum
      return chksum == (verify & kHashSerialCkMask);
    }
    return false;
  }

  /*-------------- instance methods ---------------*/

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE void IncrementalHasher<H,HashValueType,HashLength>
  :: AsyncUpdateComplete(void)
  {
    asyncInProgress = false;
  }

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE void IncrementalHasher<H,HashValueType,HashLength>
  ::Digest(HashValueType *hash) const
  {
    hasher.Digest( hash, (uint32_t) total );
  }

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE void IncrementalHasher<H,HashValueType,HashLength>
  ::Serialize(uint8_t *serial) const
  {
    // write state
    hasher.Serialize(serial);
    // write state type
    serial[kHashSerialTypeIndex] |= kHashSerialType;
    // write total
    WriteHashBytes<1>(&total, &serial[kHashSerialTotalIndex]);
    // build checksum
    checksum_t chksum = PMurHash32(serial, kHashSerialSize - kHashSerialCkSize, kHashSerialCkSeed);
    if (kHashSerialCkSize < sizeof(checksum_t)) {
      chksum ^= (chksum >> ((sizeof(checksum_t) - kHashSerialCkSize)*8));
    }
    // write checksum
    for(int i = kHashSerialCkIndex + kHashSerialCkSize ;; chksum >>=8) {
      serial[--i] = (uint8_t) chksum & 0xFF;
      if (i == kHashSerialCkIndex) break;
    }
  }

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE void IncrementalHasher<H,HashValueType,HashLength>
  ::Update(const void *data, uint32_t length)
  {
    total += (total_t) length;
    hasher.Update( data, length );
  }

  /*------------------ operators ------------------*/

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  NAN_INLINE void IncrementalHasher<H,HashValueType,HashLength>
  ::operator=(const IncrementalHasher_T& other)
  {
    hasher = other.hasher;
    total  = other.total;
  }

  /*-------------- static variables ---------------*/

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  Persistent<FunctionTemplate> IncrementalHasher<H,HashValueType,HashLength>::constructor;

  /*------------------ node init ------------------*/

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  void IncrementalHasher<H,HashValueType,HashLength>
  ::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE& target, const char* name, const char *altname)
  {
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName( Nan::New<String>(name).ToLocalChecked() );

    Local<ObjectTemplate> i_t = tpl->InstanceTemplate();
    i_t->SetInternalFieldCount(1);

    Nan::SetAccessor( i_t, Nan::New<String>("isBusy").ToLocalChecked(), GetIsBusy,
                            NULL, Local<Value>(), v8::DEFAULT,
                            static_cast<PropertyAttribute>(DontEnum | DontDelete));
    Nan::SetAccessor( i_t, Nan::New<String>("total").ToLocalChecked(), GetTotal );
   
    Nan::SetTemplate(tpl, Nan::New<String>("SERIAL_BYTE_LENGTH").ToLocalChecked(),
                          Nan::New<Int32>(kHashSerialSize),
                          static_cast<PropertyAttribute>(ReadOnly | DontDelete) );
    Nan::SetInstanceTemplate(tpl,
                          Nan::New<String>("SERIAL_BYTE_LENGTH").ToLocalChecked(),
                          Nan::New<Int32>(kHashSerialSize),
                          static_cast<PropertyAttribute>(ReadOnly | DontDelete) );
    Nan::SetPrototypeMethod(tpl, "copy",      Copy);
    Nan::SetPrototypeMethod(tpl, "digest",    Digest);
    Nan::SetPrototypeMethod(tpl, "serialize", Serialize);
    Nan::SetPrototypeMethod(tpl, "toJSON",    Serialize);
    Nan::SetPrototypeMethod(tpl, "update",    Update);

    Local<Value> fn = Nan::GetFunction(tpl).ToLocalChecked();
    constructor.Reset( tpl );
    if (altname != NULL) {
      Nan::Set(target, Nan::New<String>(name).ToLocalChecked(), fn);
      Nan::Set(target, Nan::New<String>(altname).ToLocalChecked(), fn);
    } else {
      Nan::Set(target, Nan::New<String>(name).ToLocalChecked(), fn);
    }
  }

  NAN_MODULE_INIT(Init)
  {
    IncrementalHasher<IncrementalMurmurHash3A,  uint32_t, 1>::Init(target, "MurmurHash");
  #if defined(NODE_MURMURHASH_DEFAULT_32BIT)
    IncrementalHasher<IncrementalMurmurHash128, uint64_t, 2>::Init(target, "MurmurHash128x64");
    IncrementalHasher<IncrementalMurmurHash128, uint32_t, 4>::Init(target, "MurmurHash128x86", "MurmurHash128");
  #else
    IncrementalHasher<IncrementalMurmurHash128, uint64_t, 2>::Init(target, "MurmurHash128x64", "MurmurHash128");
    IncrementalHasher<IncrementalMurmurHash128, uint32_t, 4>::Init(target, "MurmurHash128x86");
  #endif
  }
}

NODE_MODULE(murmurhashincremental, MurmurHash::Init)
