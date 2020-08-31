#pragma once

#include <nlohmann/json.hpp>
#include <variant>

#include "ipfs.hh"
#include "hash.hh"
#include "path.hh"

namespace nix {

/*
 * Mini content address
 */

/* Counterpart of FileIngestionMethod that is a struct because there is only
   one method. */
struct IsText : std::monostate { };

enum struct FileIngestionMethod : uint8_t {
    Flat,
    Recursive,
    Git,
};

struct IsIPFS : std::monostate { };

/* Compute the prefix to the hash algorithm which indicates how the files were
   ingested. */
std::string makeFileIngestionPrefix(FileIngestionMethod m);


/* Just the type of a content address. Combine with the hash itself, and we
   have a `ContentAddress` as defined below. Combine that, in turn, with info
   on references, and we have `ContentAddressWithReferences`, as defined
   further below. */
typedef std::variant<
    IsText,
    FileIngestionMethod,
    IsIPFS
> ContentAddressingMethod;

/* Compute the prefix to the hash algorithm which indicates how the files were
   ingested. */
std::string makeContentAddressingPrefix(ContentAddressingMethod m);


struct TextHash {
    Hash hash;
    bool operator ==(TextHash otherHash) const noexcept {
        return hash == otherHash.hash;
    };
    bool operator !=(TextHash otherHash) const noexcept {
        return hash != otherHash.hash;
    };
};

/// Pair of a hash, and how the file system was ingested
struct FixedOutputHash {
    FileIngestionMethod method;
    Hash hash;
    bool operator ==(FixedOutputHash otherHash) const noexcept {
        return method == otherHash.method && hash == otherHash.hash;
    };
    bool operator !=(FixedOutputHash otherHash) const noexcept {
        return method != otherHash.method && hash == otherHash.hash;
    };
    std::string printMethodAlgo() const;
};


/*
  We've accumulated several types of content-addressed paths over the years;
  fixed-output derivations support multiple hash algorithms and serialisation
  methods (flat file vs NAR). Thus, ‘ca’ has one of the following forms:

  * ‘text:sha256:<sha256 hash of file contents>’: For paths
    computed by makeTextPath() / addTextToStore().

  * ‘fixed:<r?>:<ht>:<h>’: For paths computed by
    makeFixedOutputPath() / addToStore().
*/
typedef std::variant<
    TextHash, // for paths computed by makeTextPath() / addTextToStore
    FixedOutputHash, // for path computed by makeFixedOutputPath
    IPFSHash
> ContentAddress;

std::string renderContentAddress(ContentAddress ca);

std::string renderContentAddress(std::optional<ContentAddress> ca);

ContentAddress parseContentAddress(std::string_view rawCa);

std::optional<ContentAddress> parseContentAddressOpt(std::string_view rawCaOpt);

Hash getContentAddressHash(const ContentAddress & ca);


/*
 * References set
 */

template<typename Ref>
struct PathReferences
{
    std::set<Ref> references;
    bool hasSelfReference = false;

    bool operator == (const PathReferences<Ref> & other) const
    {
        return references == other.references
            && hasSelfReference == other.hasSelfReference;
    }

    bool operator != (const PathReferences<Ref> & other) const
    {
        return references != other.references
            || hasSelfReference != other.hasSelfReference;
    }

    /* Functions to view references + hasSelfReference as one set, mainly for
       compatibility's sake. */
    StorePathSet referencesPossiblyToSelf(const Ref & self) const;
    void insertReferencePossiblyToSelf(const Ref & self, Ref && ref);
    void setReferencesPossiblyToSelf(const Ref & self, std::set<Ref> && refs);
};

template<typename Ref>
StorePathSet PathReferences<Ref>::referencesPossiblyToSelf(const Ref & self) const
{
    StorePathSet refs { references };
    if (hasSelfReference)
        refs.insert(self);
    return refs;
}

template<typename Ref>
void PathReferences<Ref>::insertReferencePossiblyToSelf(const Ref & self, Ref && ref)
{
    if (ref == self)
        hasSelfReference = true;
    else
        references.insert(std::move(ref));
}

template<typename Ref>
void PathReferences<Ref>::setReferencesPossiblyToSelf(const Ref & self, std::set<Ref> && refs)
{
    if (refs.count(self))
        hasSelfReference = true;
        refs.erase(self);

    references = refs;
}

void to_json(nlohmann::json& j, const ContentAddress & c);
void from_json(const nlohmann::json& j, ContentAddress & c);

// Needed until https://github.com/nlohmann/json/pull/211

void to_json(nlohmann::json& j, const std::optional<ContentAddress> & c);
void from_json(const nlohmann::json& j, std::optional<ContentAddress> & c);

/*
 * Full content address
 *
 * See the schema for store paths in store-api.cc
 */

// This matches the additional info that we need for makeTextPath
struct TextInfo : TextHash {
    // References for the paths, self references disallowed
    StorePathSet references;
};

struct FixedOutputInfo : FixedOutputHash {
    // References for the paths
    PathReferences<StorePath> references;
};

// pair of name and a hash of a content address
struct IPFSRef {
    std::string name;
    IPFSHash hash;

    bool operator < (const IPFSRef & other) const
    {
        return name < other.name;
        // FIXME second field
    }
};

struct IPFSInfo {
    Hash hash;
    // References for the paths
    PathReferences<IPFSRef> references;
};

typedef std::variant<
    TextInfo,
    FixedOutputInfo,
    IPFSInfo,
    IPFSHash
> ContentAddressWithReferences;

ContentAddressWithReferences contentAddressFromMethodHashAndRefs(
    Store & store, ContentAddressingMethod method, Hash && hash, PathReferences<StorePath> && refs);

ContentAddressingMethod getContentAddressMethod(const ContentAddressWithReferences & ca);
Hash getContentAddressHash(const ContentAddressWithReferences & ca);

std::string printMethodAlgo(const ContentAddressWithReferences &);

struct StorePathDescriptor {
    std::string name;
    ContentAddressWithReferences info;

    bool operator == (const StorePathDescriptor & other) const
    {
        return name == other.name;
        // FIXME second field
    }

    bool operator < (const StorePathDescriptor & other) const
    {
        return name < other.name;
        // FIXME second field
    }
};

std::string renderStorePathDescriptor(StorePathDescriptor ca);

StorePathDescriptor parseStorePathDescriptor(std::string_view rawCa);

void to_json(nlohmann::json& j, const IPFSRef & c);
void from_json(const nlohmann::json& j, IPFSRef & c);

void to_json(nlohmann::json& j, const StorePathDescriptor & c);
void from_json(const nlohmann::json& j, StorePathDescriptor & c);

/* This is needed in the from_json function for PathReferences<IPFSRef> */
void from_json(const nlohmann::json& j, std::set<IPFSRef> & c);

void to_json(nlohmann::json& j, const PathReferences<IPFSRef> & c);
void from_json(const nlohmann::json& j, PathReferences<IPFSRef> & c);

}
