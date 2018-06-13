# Discovery Feed Content Provider
Like the Search Provider and (Companion App
Service)[https://github.com/endlessm/eos-companion-app-integration], the
(Discovery Feed)[https://github.com/endlessm/eos-discovery-feed] cannot have a
dependency on any particular version of the Endless Content SDK or libraries
such as (ekncontent/libdmodel)[https://github.com/endlessm/libdmodel].

The "Discovery Feed Providers" mechanism provides a way to export a consistent
interface across all SDK versions without having to worry about ABI breaks.
The query logic is implemented on the EknServices side and the Discovery Feed
just needs to ask for content according to the "interfaces" that an app
exports.

# Requirements

## No hard dependency on ekncontent or dmodel
The Discovery Feed should not have a hard dependency on ekncontent, dmodel or
the structure of the underlying content database in an app. The same interface
should be exported for each SDK version.

## Automatic discovery of supported Discovery Feed apps.
The Discovery Feed should not have to inspect an app to determine what
"interfaces" it supports (whether that is showing videos, news articles or
special content such as "word of the day"). Instead, the app should provide
that information that the Discovery Feed can parse.

## Efficient queries
The Discovery Feed queries should leverage the structure of the underlying
content databases so as to avoid wasting time creating models for entries.
Thus if filtering can be done on the database side, it should be done there.

## Rotation of content
The Discovery Feed queries should ensure that fresh content is shown every day.

# Solution

## Content Provider Files
By default, an App will not have Discovery Feed support enabled. It is **opt in**.

To opt into discovery feed support, an app SHOULD install the following file
into
`$XDG_DATA_DIRS/share/eos-discovery-feed/content-providers/[app-id]-discovery-feed-content-provider.ini`:

    [Discovery Feed Content Provider]
    DesktopId=[desktop id]
    BusName=[bus name which has object exporting
             com.endlessm.DiscoveryFeed interface]
    ObjectPath=[object path for object exporting
                com.endlessm.DiscoveryFeed interface]
    AppID=[app id]
    SupportedInterfaces=[semicolon delimited list of
                         supported Discovery Feed interfaces]
    Version=1

    [Load Item Provider]
    ObjectPath=[object path for object exporting com.endlessm.KnowledgeSearch]

### Discovery Feed Content Provider section
An app does not need to use `EknServices` to export its content in the
Discovery Feed, however it is recommended that if they are build using the
Endless Content SDK, that they do so, since it will prevent waking up
unnecessary processes and ensures that apps appear to export a consistent API.

To make use of `EknServices`, the app SHOULD specify the following properties
in this section:

    BusName=com.endlessm.EknServicesN.SearchProviderVN
    ObjectPath=/com/endlessm/EknServicesN/SearchProviderVN/[encoded_app_id]

`EknServicesN` `SearchProviderVN` should be replaced with the following
depending on the SDK version in use:

| SDK Version | Bus Name     | Search Provider Name|
|:-----------:|:------------:|:-------------------:|
| 1           | EknServices  | SearchProviderV1    |
| 2           | EknServices2 | SearchProviderV2    |
| 3           | EknServices2 | SearchProviderV2    |
| 4           | EknServices3 | SearchProviderV2    |

The `[encoded_app_id]` is encoded according to the systemd D-Bus object name
encoding scheme.

`SupportedInterfaces` is a comma delimited list containing any of the
following depending on what the app should export to the Discovery Feed:
 * `com.endlessm.DiscoveryFeedContent`: Exposes a single
   `ArticleCardDescriptions()` method, returning five `EknArticleObject`
   tagged articles, rotating such that "fresh" content, even if it is old, is
   shown once per day.
 * `com.endlessm.DiscoveryFeedNews`: Exposes a single `GetRecentNews()`
   method, returning five `EknArticleObject` tagged articles, ordered from
   newest to oldest.
 * `com.endlessm.DiscoveryFeedWord`: Exposes a single `GetWordOfTheDay()`
   method, returning a single `EknArticleObject` tagged article, assumed to be
   the "word of the day".
 * `com.endlessm.DiscoveryFeedQuote`: Exposes a single `GetQuoteOfTheDay()`
   method, returning a single `EknArticleObject` tagged article, assumed to be
   the "quote of the day".
 * `com.endlessm.DiscoveryFeedArtwork`: Exposes a single
   `ArtworkCardDescriptions()` method, returning one `EknArticleObject` tagged
   articles, assumed to be artwork pieces with temporal coverage, rotating
   such that "fresh" content, even if it is old, is shown once per day.
 * `com.endlessm.DiscoveryFeedVideo`: Exposes a single `GetVideos()` method,
   returning a single `EknMediaObject` tagged video, rotating such that
   "fresh" content, even if it is old, is shown once per day.


### Load Item section
If the app supports the `com.endlessm.KnowledgeSearch` interface, it should
expose a `Load Item Provider` section in its content provider. The single
property, `ObjectPath` specifies an object path on the app's bus name where an
object exporting the `com.endlessm.KnowledgeSearch` interface can be found.
When this card is clicked in the Discovery Feed, the relevant article will
open in the app.

## Database Usage
Ideally we want to show fresh content in the Discovery Feed every day. The way
this is achieved is to choose items from the feed based on the day of the
week. Instead of querying the entire set of data and then manually skipping
until the Nth item in the list, we use the "offset" parameter to ensure that
this is done on the Xapian side (which avoids all the overhead of extracting
metadata for each item and then throwing it away - Xapian knows how big each
page is and can skip ahead).

However, this creates potential problem which is that the requested "offset"
may be greater than the number of articles in the collection, causing the
query to return less than the desired number or no results at all.

Thus, on all queries we use a helper, `query_with_wraparound_offset`. This
takes an existing `DmQuery` object and copies it, setting the `limit`
parameter to 0. The initially returned result set will be empty, however, from
that we can determine the number of results in the collection had no limit
been applied by checking the `upper-bound` property. From there, the offset
can "wrap around" so as to never overrun the availble number of items in the
collection.

## API stability
As with the [Metadata Provider](/docs/MetadataProvider.md), new keys may be
added to an existing interface's returned Variant Dict, but old keys will not
be removed unless a new interface name is created.

The parameters for any methods exposed on an interface never changes over an
interface' lifetime. No methods are added or removed over the course of an
interface' lifetime either.

## Interfaces

### com.endlessm.DiscoveryFeedContent
Used for apps that have encycolpedic or informational content such that no
article is inherently "fresher" than another. Content is sorted by date of
ingestion and a query offset is applied based on which day of the year it is.
Exposes a single method, `ArticleCardDescriptions()` which returns the
following as a string-to-string dictionary `a{ss}`:

    {
      "title": [either a "hook title" or the article title],
      "synopsis": [either empty or the article synopsis],
      "id": [the content URI, not stable across app versions],
      "last-modified-date": [ISO8601 formatted date specifying
                             the last time the article was modified],
      "thumbnail-uri": [the content URI for the article thumbnail],
      "content-type": [MIME type for the content]
    }

#### Hook Titles
Apps exporting the `com.endlessm.DiscoveryFeedContent` may have a "hook title"
for some content. This is achieved by setting the `blurbs` entry in
`discoveryFeedContent` in the metadata with an array of alternate titles for
the content. So called "hook titles" are alternate titles to the article title
which are supposed to provide an interesting hint for the article. For
instance, the "hook title" for Barack Obama might read "He was the first
African American man to become President of the United States".

When a hook title is displayed, the synopsis of the content will be hidden.
This is enforced by EknServices, which will set the `synopsis` entry to be
blank.

### com.endlessm.DiscoveryFeedNews
Used for apps that have frequently updated content such that new articles are
inherently "fresher" than older one and should be displayed first. Content is
sorted by date of ingestion. Exposes a single method, `GetRecentNews()` which
returns the following as a string-to-string dictionary `a{ss}`:

    {
      "title": [the article title],
      "synopsis": [article synopsis],
      "id": [the content URI, not stable across app versions],
      "last-modified-date": [ISO8601 formatted date specifying the
                             last time the article was modified],
      "thumbnail-uri": [the content URI for the article thumbnail],
      "content-type": [MIME type for the content]
    }

### com.endlessm.DiscoveryFeedArtwork
Used for apps that have "gallery" content with short descriptions, such that
the image is the most prominent part of an article. No article should be
inherently "fresher" than another. Content is sorted by date of ingestion and
a query offset is applied based on which day of the year it is. Exposes a
single method, `ArtworkCardDescriptions()` which returns the following as a
string-to-string dictionary `a{ss}`:

    {
      "title": [the article title],
      "synopsis": [article synopsis],
      "id": [the content URI, not stable across app versions],
      "last-modified-date": [ISO8601 formatted date specifying
                             the last time the article was modified],
      "thumbnail-uri": [the content URI for the article thumbnail],
      "author": [first author name],
      "first-date": [first date the artwork was known to the public]
      "content-type": [MIME type for the content]
    }

### com.endlessm.DiscoveryFeedWord
Used for apps that expose a "word of the day" in each article with a
definition and part of speech. No article should be inherently "fresher" than
another. Content is sorted by date of ingestion and a query offset is applied
based on which day of the year it is. Exposes a single method,
`GetWordOfTheDay()` which returns the following as a string-to-string
dictionary `a{ss}`:

    {
      "word": [the relevant word],
      "definition": [the word's definition],
      "part-of-speech": [what part of speech the word is (noun, verb)]
    }

### com.endlessm.DiscoveryFeedQuote
Used for apps that expose a "quote of the day" in each article with an author.
No article should be inherently "fresher" than another. Content is sorted by
date of ingestion and a query offset is applied based on which day of the year
it is. Exposes a single method, `GetQuoteOfTheDay()` which returns the
following as a string-to-string dictionary `a{ss}`:

    {
      "quote": [the relevant quote],
      "author": [who said the quote],
      "part-of-speech": [what part of speech the word is (noun, verb)]
    }

### com.endlessm.DiscoveryFeedVideo
Used for apps that have "video" content with short descriptions, such that the
video is the most prominent part of an article. No article should be
inherently "fresher" than another. Content is sorted by date of ingestion and
a query offset is applied based on which day of the year it is. Exposes a
single method, `GetVideos()` which returns the following as a string-to-string
dictionary `a{ss}`:

    {
      "title": [the video title],
      "duration": [string, how long the video is in seconds],
      "id": [the content URI, not stable across app versions],
      "thumbnail-uri": [the content URI for the article thumbnail],
      "content-type": [MIME type for the content]
    }