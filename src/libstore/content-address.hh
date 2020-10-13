#pragma once

#include <variant>
#include "hash.hh"
#include "path.hh"

namespace nix {

/*
 * Content addressing method
 */

/* We only have one way to hash text with references, so this is a single-value
   type, mainly useful with std::variant.
*/
struct TextHashMethod : std::monostate { };

enum struct FileIngestionMethod : uint8_t {
    Flat,
    Recursive,
};

/* Compute the prefix to the hash algorithm which indicates how the files were
   ingested. */
std::string makeFileIngestionPrefix(FileIngestionMethod m);


/* Just the type of a content address. Combine with the hash itself, and we
   have a `ContentAddress` as defined below. Combine that, in turn, with info
   on references, and we have `ContentAddressWithReferences`, as defined
   further below. */
typedef std::variant<
    TextHashMethod,
    FileIngestionMethod
> ContentAddressMethod;

/* Parse and pretty print the algorithm which indicates how the files
   were ingested, with the the fixed output case not prefixed for back
   compat. */

std::string makeContentAddressingPrefix(ContentAddressMethod m);

ContentAddressMethod parseContentAddressingPrefix(std::string_view & m);

/* Parse and pretty print a content addressing method and hash in a
   nicer way, prefixing both cases. */

std::string renderContentAddressMethodAndHash(ContentAddressMethod cam, HashType ht);

std::pair<ContentAddressMethod, HashType> parseContentAddressMethod(std::string_view caMethod);


/*
 * Mini content address
 */

struct TextHash {
    Hash hash;
};

/// Pair of a hash, and how the file system was ingested
struct FixedOutputHash {
    FileIngestionMethod method;
    Hash hash;
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
    FixedOutputHash // for path computed by makeFixedOutputPath
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

typedef std::variant<
    TextInfo,
    FixedOutputInfo
> ContentAddressWithReferences;

ContentAddressWithReferences contentAddressFromMethodHashAndRefs(
    ContentAddressMethod method, Hash && hash, PathReferences<StorePath> && refs);

ContentAddressMethod getContentAddressMethod(const ContentAddressWithReferences & ca);
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

}
