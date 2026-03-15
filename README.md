# cblog — Static Blog Generator in C

A production-ready static site generator written in C17. Converts Markdown blog posts into a complete static HTML/CSS/JS site suitable for GitHub Pages deployment.

## Features

- **Markdown to HTML** — Full markdown support including headers, links, code blocks, images, lists, tables, and inline HTML
- **Template Engine** — Lightweight templating with variable interpolation, loops, and conditionals
- **Themes** — Pluggable theme system; switch themes without recompiling
- **RSS Feed** — Automatic RSS/XML feed generation
- **Local Server** — Built-in multi-threaded HTTP server for previewing your site
- **GitHub Pages** — Output is fully compatible with GitHub Pages (relative links, `.nojekyll`)

## Build

```bash
make
```

Requires a C17-compatible compiler (GCC 8+, Clang 5+) and POSIX (Linux / macOS).

## Install

```bash
sudo make install
```

## Quick Start

```bash
# Create a new blog project
cblog init myblog
cd myblog

# Create a new post
cblog new "My First Post"

# Build the static site
cblog build

# Preview locally
cblog serve --port 8080
```

## Commands

| Command | Description |
|---|---|
| `cblog init <name>` | Initialize a new blog project |
| `cblog new "Title"` | Create a new markdown post |
| `cblog page "Title"` | Create a new static page (not in blog feed) |
| `cblog build` | Build the static site into `public/` |
| `cblog serve --port <port>` | Serve `public/` on a local HTTP server |
| `cblog theme set <name>` | Switch the active theme |

## Tags Index

The build generates a `tags.html` page that lists **all tags** with their associated posts grouped underneath. If a post has multiple tags, it appears under each one. A link to this page is included in the navigation bar between Archive and RSS.

The tags index page can be customized by creating a `tags_index.html` template in your theme's `templates/` directory. Available template variables:

- **`tags`** — list of tag objects, each with:
  - `tag.name` — tag display name
  - `tag.slug` — URL-safe tag name
  - `tag.url` — link to the individual tag page (`tags/<slug>.html`)
  - `tag.count` — number of posts with this tag
  - `tag.posts` — list of posts, each with `post.title`, `post.date`, `post.url`

## Static Pages

Static pages are standalone pages that use the same markdown and templates as blog posts but are **excluded from the blog feed, archive, tags, and RSS**. Use them for reference material, about pages, guides, etc.

### Creating a page

```bash
cblog page "Setup Guide for Topic A"
```

This creates `pages/setup-guide-for-topic-a.md` with a simplified front matter (no tags or draft fields by default):

```markdown
---
title: "Setup Guide for Topic A"
date: 2026-03-07
slug: setup-guide-for-topic-a
---

Write your page content here.
```

You can optionally add `draft: true` to the front matter to exclude it from the build.

### Where pages are output

Pages are rendered to `public/pages/`, keeping the output directory clean:

```
public/
├── index.html              ← blog feed (no pages here)
├── archive.html            ← archive (no pages here)
├── tags.html               ← tags index (no pages here)
├── rss.xml                 ← RSS feed (no pages here)
├── pages/
│   └── setup-guide-for-topic-a.html   ← your static page
├── posts/
│   └── my-blog-post.html
└── ...
```

### Linking to pages from blog posts

In any blog post markdown, link to a static page using its slug:

```markdown
For setup instructions, see the [Setup Guide](/pages/setup-guide-for-topic-a.html).
```

Or with `base_url` if deploying to a subdirectory:

```markdown
See the [Setup Guide](/repo-name/pages/setup-guide-for-topic-a.html).
```

### Custom page template

By default, pages use the `post.html` theme template. If you want a different layout for pages, create a `page.html` template in your theme's `templates/` directory. It supports the same variables as `post.html` (except `tags` and `tag_list` which will be empty).

## Configuration

`config.json` in the project root:

```json
{
    "site_title": "My Blog",
    "author": "Your Name",
    "base_url": "",
    "theme": "default",
    "pagination_size": 10,
    "output_dir": "public",
    "enable_rss": true,
    "enable_seo": false,
    "enable_simple_analytics": false,
    "site_description": ""
}
```

| Field | Description |
|---|---|
| `site_title` | Name of your blog |
| `author` | Author name shown in posts and footer |
| `base_url` | URL prefix for deployment (empty for root, `/repo-name` for GitHub Pages project sites) |
| `theme` | Active theme directory name |
| `pagination_size` | Max posts shown on the index page |
| `output_dir` | Build output directory |
| `enable_rss` | Generate `rss.xml` feed |
| `enable_seo` | Inject meta description, Open Graph, and Twitter Card tags into every page |
| `enable_simple_analytics` | Inject the [Simple Analytics](https://www.simpleanalytics.com/) script into every page |
| `site_description` | Default meta description used when a page-specific one is not available |

## SEO (Search Engine Optimization)

When `enable_seo` is set to `true` in `config.json`, cblog automatically injects the following into the `<head>` of every generated page:

- **Meta description** — For blog posts, this is auto-generated from the first ~160 characters of the post content (HTML stripped, whitespace normalized). For other pages (index, archive, tags), the `site_description` from config is used.
- **Open Graph tags** — `og:title`, `og:description`, `og:type`, `og:url`, `og:site_name`
- **Twitter Cards** — `twitter:card`, `twitter:title`, `twitter:description`

### Enabling SEO

Edit `config.json`:

```json
{
    "enable_seo": true,
    "site_description": "A blog about programming, systems design, and technology"
}
```

Then rebuild:

```bash
cblog build
```

### Generated output example

For a blog post titled "Learning Rust", the `<head>` will include:

```html
<meta name="description" content="Learning Rust Rust is a systems programming language focused on safety and performance...">
<meta property="og:title" content="Learning Rust">
<meta property="og:description" content="Learning Rust Rust is a systems programming language focused on safety and performance...">
<meta property="og:type" content="article">
<meta property="og:url" content="/posts/learning-rust.html">
<meta property="og:site_name" content="My Blog">
<meta name="twitter:card" content="summary">
<meta name="twitter:title" content="Learning Rust">
<meta name="twitter:description" content="Learning Rust Rust is a systems programming language focused on safety and performance...">
```

### Custom SEO in themes

If you create a custom theme, add the SEO block to your `layout.html` `<head>`:

```html
{% if enable_seo %}
<meta name="description" content="{{ meta_description }}">
<meta property="og:title" content="{{ og_title }}">
<meta property="og:description" content="{{ og_description }}">
<meta property="og:type" content="{{ og_type }}">
<meta property="og:url" content="{{ og_url }}">
<meta property="og:site_name" content="{{ site_title }}">
<meta name="twitter:card" content="{{ twitter_card }}">
<meta name="twitter:title" content="{{ twitter_title }}">
<meta name="twitter:description" content="{{ twitter_description }}">
{% endif %}
```

These variables are only set when `enable_seo` is `true` in config — otherwise the block is skipped cleanly.

## Simple Analytics

[Simple Analytics](https://www.simpleanalytics.com/) is a privacy-friendly, cookieless analytics service. When enabled, cblog injects their tracking script just before the closing `</body>` tag on every generated page.

### 1. Create a Simple Analytics account

Sign up at [simpleanalytics.com](https://www.simpleanalytics.com/) and add your site's domain. No additional configuration is required on their end — the script loads automatically once the domain is registered.

### 2. Enable in config.json

```json
{
    "enable_simple_analytics": true
}
```

Then rebuild:

```bash
cblog build
```

### What gets injected

When `enable_simple_analytics` is `true`, the following script tag is added just before `</body>` on every page:

```html
<script async defer src="https://scripts.simpleanalyticscdn.com/latest.js"></script>
```

Setting `enable_simple_analytics` to `false` (the default) omits the script entirely — no requests are made to Simple Analytics and no tracking occurs.

### Custom theme support

If you create a custom theme, add the following block to your `layout.html` just before `</body>`:

```html
{% if enable_simple_analytics %}
<script async defer src="https://scripts.simpleanalyticscdn.com/latest.js"></script>
{% endif %}
```

## Themes

Themes live in `themes/<theme_name>/` and contain:

```
themes/mytheme/
├── theme.json
├── templates/
│   ├── layout.html
│   ├── index.html
│   ├── post.html
│   ├── archive.html
│   └── tag.html
└── assets/
    ├── css/
    │   └── style.css
    └── js/
        └── main.js
```

Switch themes at any time without recompiling:

```bash
cblog theme set mytheme
cblog build
```

## Creating Your Own Theme

### 1. Create the theme directory

```bash
mkdir -p themes/mytheme/templates
mkdir -p themes/mytheme/assets/css
mkdir -p themes/mytheme/assets/js
```

### 2. Create `theme.json`

Every theme needs a `theme.json` metadata file in its root:

```json
{
    "name": "mytheme",
    "version": "1.0.0",
    "author": "Your Name",
    "description": "A clean minimal theme"
}
```

### 3. Create templates

You must provide **5 template files** in `templates/`:

#### `layout.html` — Base wrapper for every page

This is the outer HTML shell. Use `{{{ content }}}` (triple braces for raw/unescaped HTML) to mark where page content is injected.

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{{ page_title }} — {{ site_title }}</title>
    <link rel="stylesheet" href="{{ base_url }}/assets/css/style.css">
    <link rel="alternate" type="application/rss+xml"
          title="{{ site_title }}" href="{{ base_url }}/rss.xml">
</head>
<body>
    <header>
        <nav>
            <a href="{{ base_url }}/">{{ site_title }}</a>
            <a href="{{ base_url }}/">Home</a>
            <a href="{{ base_url }}/archive.html">Archive</a>
            <a href="{{ base_url }}/rss.xml">RSS</a>
        </nav>
    </header>
    <main>
        {{{ content }}}
    </main>
    <footer>
        <p>&copy; {{ year }} {{ author }}</p>
    </footer>
    <script src="{{ base_url }}/assets/js/main.js"></script>
</body>
</html>
```

#### `index.html` — Home page listing posts

```html
<h1>{{ site_title }}</h1>
<p>Written by {{ author }}</p>

{% for post in posts %}
<article>
    <h2><a href="{{ post.url }}">{{ post.title }}</a></h2>
    <time datetime="{{ post.date }}">{{ post.date }}</time>
    <p>{{ post.excerpt }}</p>
    <a href="{{ post.url }}">Read more</a>
</article>
{% endfor %}
```

#### `post.html` — Individual blog post

```html
<article>
    <h1>{{ title }}</h1>
    <time datetime="{{ date }}">{{ date }}</time>
    <span>by {{ author }}</span>

    <div class="post-content">
        {{{ content }}}
    </div>

    {% if tags %}
    <div class="tags">
        {% for tag in tag_list %}
        <a href="{{ tag.url }}">{{ tag.name }}</a>
        {% endfor %}
    </div>
    {% endif %}
</article>
```

#### `archive.html` — Chronological listing of all posts

```html
<h1>Archive</h1>
{% for post in posts %}
<div>
    <time datetime="{{ post.date }}">{{ post.date }}</time>
    <a href="{{ post.url }}">{{ post.title }}</a>
</div>
{% endfor %}
```

#### `tag.html` — Posts filtered by tag

```html
<h1>Tag: {{ tag_name }}</h1>
{% for post in posts %}
<div>
    <time datetime="{{ post.date }}">{{ post.date }}</time>
    <a href="{{ post.url }}">{{ post.title }}</a>
</div>
{% endfor %}
```

### 4. Add styles and scripts

Place your CSS in `assets/css/style.css` and optional JavaScript in `assets/js/main.js`. These are copied into `public/assets/` at build time.

### 5. Template syntax reference

| Syntax | Description |
|---|---|
| `{{ variable }}` | Output a variable (HTML-escaped) |
| `{{{ variable }}}` | Output raw/unescaped HTML (use for `content`) |
| `{% for item in list %}...{% endfor %}` | Loop over a list |
| `{{ item.field }}` | Access a field inside a loop item |
| `{% if variable %}...{% endif %}` | Conditional block |
| `{% if variable %}...{% else %}...{% endif %}` | Conditional with else |

### 6. Available template variables

**In `layout.html`:**

| Variable | Description |
|---|---|
| `{{ page_title }}` | Title of the current page |
| `{{ site_title }}` | Site title from `config.json` |
| `{{ author }}` | Author from `config.json` |
| `{{ base_url }}` | Base URL from `config.json` |
| `{{ year }}` | Current year |
| `{{{ content }}}` | Inner page HTML (from index/post/archive/tag templates) |

**In `index.html` and `archive.html`:**

| Variable | Description |
|---|---|
| `{{ site_title }}`, `{{ author }}`, `{{ base_url }}` | Site-level variables |
| `posts` | List of posts, each with `.title`, `.date`, `.slug`, `.url`, `.excerpt` |

**In `post.html`:**

| Variable | Description |
|---|---|
| `{{ title }}` | Post title |
| `{{ date }}` | Post date |
| `{{ author }}` | Author |
| `{{ slug }}` | Post slug |
| `{{ tags }}` | Comma-separated tag string |
| `{{{ content }}}` | Rendered HTML content of the post |
| `tag_list` | List of tags, each with `.name` and `.url` |

**In `tag.html`:**

| Variable | Description |
|---|---|
| `{{ tag_name }}` | Name of the tag |
| `posts` | List of posts with that tag (same fields as index) |

### 7. Activate your theme

```bash
cblog theme set mytheme
cblog build
```

## GitHub Pages Deployment

### Option A: Deploy from a `gh-pages` branch

This is the most common approach. You push only the `public/` output to a dedicated branch.

**Initial setup (one-time):**

```bash
# From your blog project root, build the site
cblog build

# Initialize a git repo inside public/ pointing to your GitHub repo
cd public
git init
git remote add origin git@github.com:YOUR_USERNAME/YOUR_REPO.git
git checkout -b gh-pages
git add -A
git commit -m "Initial deploy"
git push -u origin gh-pages
```

**Subsequent deploys:**

```bash
# Rebuild the site
cblog build

# Push updates
cd public
git add -A
git commit -m "Update site"
git push origin gh-pages
```

**Enable GitHub Pages:**

1. Go to your repository on GitHub
2. Navigate to **Settings** > **Pages**
3. Under **Source**, select the `gh-pages` branch and `/ (root)` folder
4. Click **Save**
5. Your site will be live at `https://YOUR_USERNAME.github.io/YOUR_REPO/`

### Option B: Deploy from `docs/` on `main` branch

If you prefer keeping everything in one branch, configure cblog to output into `docs/`:

```bash
# Edit config.json and change output_dir
```

```json
{
    "output_dir": "docs"
}
```

```bash
cblog build
git add -A
git commit -m "Update site"
git push origin main
```

Then in GitHub **Settings** > **Pages**, select the `main` branch and `/docs` folder.

### Option C: Using a deploy script

Create a `deploy.sh` at your project root for one-command deploys:

```bash
#!/bin/bash
set -e

echo "Building site..."
cblog build

cd public

if [ ! -d .git ]; then
    git init
    git remote add origin git@github.com:YOUR_USERNAME/YOUR_REPO.git
    git checkout -b gh-pages
fi

git add -A
git commit -m "Deploy: $(date '+%Y-%m-%d %H:%M:%S')"
git push -f origin gh-pages

echo "Deployed successfully!"
```

```bash
chmod +x deploy.sh
./deploy.sh
```

### Setting `base_url` for GitHub Pages

If your site is hosted at `https://username.github.io/repo-name/` (i.e. not a custom domain), you **must** set `base_url` in `config.json` so that CSS, JS, and links resolve correctly:

```json
{
    "base_url": "/repo-name"
}
```

If using a custom domain or a user site (`username.github.io`), leave `base_url` empty:

```json
{
    "base_url": ""
}
```

### What cblog generates for GitHub Pages

- **`.nojekyll`** — Automatically created in `public/` to prevent GitHub from processing the site with Jekyll
- **Relative URLs** — All internal links use `base_url` so they work under any path prefix
- **Pure static output** — No server-side logic; everything is plain HTML/CSS/JS

## License

MIT
