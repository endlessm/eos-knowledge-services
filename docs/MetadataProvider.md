# Purpose and Problem Definition
Previously, the [Companion App Service](https://github.com/endlessm/eos-companion-app-integration)
linked to the Platform SDK and [ekncontent/dmodel](https://github.com/endlessm/libdmodel)
directly. This was not a great situation to be in, since it meant that we had
a hard-dependency on apps from a single SDK version. It is against the SDK’s
design and Flatpak's design to try and support apps from multiple SDK versions
concurrently - the database schema and ekncontent API/ABI is liable to change
between SDK versions.

The Companion App Service thus needed to make use of the standard mechanism
used to achieve multi-SDK compatibility, which is EknServices. That imports
along with it several other concerns.

The way EknServices was used, which was to provide a narrow
consumer-specific API on top of the existing query interface, had proven to be
a poor developer experience. In the case of the Discovery Feed, code was often
duplicated within EknServices itself to support slightly varying queries
because the external API provided was not flexible enough. This led to the
kind of problems that code duplication tends to cause - inconsistency in what
should be shared implementation and a "write-twice, debug-in-two-places"
problem in development. It was much nicer if EknServices exposed a query
API that was stable across SDK versions but still flexible enough to prevent
the need to constantly make changes to EknServices to implement features in
the Discovery Feed or Companion App Service.

# Requirements

## Don’t break the existing apps, Discovery Feed, Shell Search Providers or Companion App Service
Whatever happens, we shouldn’t break these things with an update. To the
extent that they might rely on an old API, any new implementation here should
provide the old API and satisfy all the old API’s implicit contracts.

## Companion App Service not dependent on apps from a particular SDK version
Right now, the Companion App Service is linked directly to a specific SDK
version. This is highly undesirable for the reasons stated above. Any new
implementation should not require that the Companion App Service has a
dependency on apps from a specific SDK version.

Of course, this doesn’t prevent us from having the Companion App Service
flatpak linked to a particular SDK version, but it should still work with all
apps regardless of what version it is linked to.

## Discovery Feed not dependent on a particular SDK version
As above, the Discovery Feed should not gain a dependency on a particular SDK
version. It doesn’t have that right now, but any new solution shouldn’t import
such a dependency.

## Stable API
Once an API is implemented in EknServices, it should not change, except if any
embedded version number in the name of the API is bumped. To the extent that
the underlying SDK changes, the external facing API in EknServices should hide
these changes as much as possible.

## Less Development Churn
Any new solution should optimize in favor of reducing developer churn as
opposed to providing a narrow API for a particular use case. Ideally,
implementing new features in either the Discovery Feed or Companion App
Service should not require subsequent changes in EknServices. Thus, the
exposed API needs to be flexible enough to deal with anticipated future needs.

# Solution

## Flexible Query API for EknServices
In order to satisfy
[Companion App Services not dependent on a particular version](#companion-app-service-not-dependent-on-apps-from-a-particular-sdk-version)
and [Less Development Churn](#less-development-churn) a new API is proposed
for EknServices called MetadataProvider.

MetadataProvider closely mirrors the current form of EkncQueryObject and
exposes the query parameters which are most useful for performing general
searches and filtering for content in a database. It is expected that as
EkncQueryObject (or DmQuery in future) changes, the two APIs will diverge
slightly. To that extent, MetadataProvider should attempt as best as possible
to provide its functionality on top of any API change. MetadataProvider has a
method Query taking an aa{sv} (an array of query descriptors) and each query
descriptor will support the following optional query parameters:

    {
      "search-terms": String (s) to be used with the free-text search terms to
                      be used during the query.
      "tags-match-any": A strv (as) with a list of tags that any matching
                        content objects must have at least one of
      "tags-match-all": A strv (as) with a list of tags that any matching
                        content objects must have all of
      "limit": An integer indicating the number of results a most to return. If
               not specified, there will be no limit
      "offset": An integer indicating the offset into the query results.
      "sort": A string specifying what the query should be sorted by. Valid
              values are:
      "relevance": Sort by relevance ranking with exact title matches weighted
                   most heavily
      "sequence-number": Sort by article page ranking
      "date": Sort by date of publication
      "alphabetical": Sort alphabetically
      "order": A string specifying how sorted query results should be ordered.
               Valid values are “ascending” and “descending”.
    }

New properties will not be added to a query object during its lifetime. In
order to do that, unknown query properties would have to be ignored which
would cause changes in query behaviour across installed versions that may
break the expectations of callers.

The method returns a tuple of (shards, array of (result_metadata, models)),
(asa(iaa{sv})). The array of (result_metadata, models) is the result of each
corresponding query descriptor passed in Query. In the event that any single
query fails, the whole request fails. In the event that a consumer needs to
look up more information about a given ID, such as its data or metadata, it
can open each shard provided in the "shards" using eos-shard to find it. The
models are an array of dictionaries with metadata about each content object
matched in the query. Note that unlike the query object, new properties may be
added to this object over the course of its lifetime, so implementations
should check if a property is present before attempting to use it. Properties
are:

    {
      "title": The title of the content object.
      "content_type": The MIME type of the underlying content data.
      "language": The language code of the content.
      "synopsis": A brief description of the content.
      "last_modified_date": Date (ISO8061) when the content was
                            last modified.
      "original_title": The original title this content had.
      "original_uri": The URI that this content was sourced from.
      "license": License for this content.
      "copyright_holder": Who holds the copyright for this content.
      "thumbnail_uri": An ID specifying the location in one
                      of the returned shards where data for
                      the thumbnail for this content is stored.
      "id": An ID specifying the location in one of the
            returned shards where data for this content is
            stored.
      "child_tags": If this content object model represents a
                    set, the tags which should be queried as part
                    of "tags-match-any" in Query above to find
                    content objects inside this set.
      "tags": The tags for this content object model, which allow
              it to be associated with a set via "child_tags" or
              provide metadata about the type of content.
      "temporal_coverage": The dates this content covers.
      "discovery_feed_content": An opaque struct with content for
                                the discovery feed.
    }

Properties of "result_metadata" are as follows:

    {
      "upper_bound": the number of models that would be returned
                    in a search result if "limit" had not been
                    applied.
    }

The interface also has a Shards method that just returns a strv (as) of shard
paths to be opened with eos-shard. Callers can use this method if they already
know an ID and corresponding app that they want to read data for.

Note that in initial implementations of this interface, only one query at will
be supported in the array, but clients should behave as though multiple
queries are supported in the array.

## Companion App Service - Use Session Bus
The Companion App Service will use its own private session bus, which will
allow it to autostart eos-knowledge-services for the companion-app-helper
user. This was done in https://github.com/endlessm/eos-companion-app-integration/pull/72

## Companion App Service - Use EknServices
The Companion App Service should stop using ekncontent and instead use
EknServices to do all of its queries. This will be implemented through an
abstraction layer which can be mocked out at test time.
See https://github.com/endlessm/eos-companion-app-integration/pull/74

## Companion App Service - Detect which EknServices to use
Right now the Companion App Service assumes that verison 3 is in use. It
should look at the flatpak metadata to work out which EknServices to use based
on the SDK the app is linked to.
