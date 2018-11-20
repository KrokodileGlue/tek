#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

struct object {
	enum object_type {
		OBJECT_NUMBER,
		OBJECT_INDIRECT_REFERENCE,
		OBJECT_NAME,
		OBJECT_DICTIONARY,
		OBJECT_ARRAY,
		OBJECT_STRING,
		OBJECT_STREAM,
		OBJECT_BOOLEAN,
		OBJECT_NULL
	} type;

	union {
		double n;
		char *s;

		/*
		 * The indirect reference: on (Object Number) must be
		 * a positive integer and GN (Generation Number) must
		 * be a non-negative integer.
		 */
		struct {
			unsigned on, gn;
		};

		struct {
			char **key;
			struct object **dict;
		};
	};

	size_t len, loc, addr;
};

struct page {
	enum {
		PDF_PAGE_SIZE_LETTER,
		PDF_PAGE_SIZE_LEGAL,
		PDF_PAGE_SIZE_A3,
		PDF_PAGE_SIZE_A4,
		PDF_PAGE_SIZE_A5,
		PDF_PAGE_SIZE_B4,
		PDF_PAGE_SIZE_B5,
		PDF_PAGE_SIZE_EXECUTIVE,
		PDF_PAGE_SIZE_US4x6,
		PDF_PAGE_SIZE_US4x8,
		PDF_PAGE_SIZE_US5x7,
		PDF_PAGE_SIZE_COMM10,
		PDF_PAGE_SIZE_EOF
	} size;

	enum {
		PDF_PAGE_PORTRAIT,
		PDF_PAGE_LANDSCAPE
	} direction;

	struct object *data, *contents;
};

struct pdf {
	enum {
		PDF_COMP_NONE     = 1 << 0,
		PDF_COMP_TEXT     = 1 << 1,
		PDF_COMP_IMAGE    = 1 << 2,
		PDF_COMP_METADATA = 1 << 3,
		PDF_COMP_ALL      = 7
	} compression;

	enum {
		PDF_PAGE_MODE_USE_NONE,
		PDF_PAGE_MODE_USE_OUTLINE,
		PDF_PAGE_MODE_USE_THUMBS,
		PDF_PAGE_MODE_FULL_SCREEN,
		PDF_PAGE_MODE_EOF
	} page_mode;

	struct page **page;
	unsigned num_page;

	struct object **object;
	unsigned num_object;
};

static struct object *
object_new(enum object_type type)
{
	struct object *o = malloc(sizeof *o);
	memset(o, 0, sizeof *o);
	o->type = type;
	return o;
}

static struct object *
object_new_name(const char *s)
{
	struct object *o = object_new(OBJECT_NAME);
	o->s = strdup(s);
	return o;
}

static struct object *
object_new_indirect_reference(int on, int gn)
{
	struct object *o = object_new(OBJECT_INDIRECT_REFERENCE);
	o->on = on, o->gn = gn;
	return o;
}

static struct object *
object_new_number(double n)
{
	struct object *o = object_new(OBJECT_NUMBER);
	return o->n = n, o;
}

static struct object *
object_copy(struct object *o)
{
	struct object *r = object_new(o->type);

	switch (o->type) {
	case OBJECT_NAME:
		r->s = strdup(o->s);
		break;
	case OBJECT_INDIRECT_REFERENCE:
		r->on = o->on;
		r->gn = o->gn;
		break;
	default:
		fprintf(stderr, "Could not copy object %u\n", o->type);
		break;
	}

	return r;
}

static void
dictionary_add(struct object *o, const char *s, struct object *v)
{
	o->key = realloc(o->key, (o->len + 1) * sizeof *o->key);
	o->dict = realloc(o->dict, (o->len + 1) * sizeof *o->dict);
	o->key[o->len] = strdup(s);
	o->dict[o->len++] = v;
}

static void
pdf_add_object(struct pdf *p, struct object *o)
{
	p->object = realloc(p->object, (p->num_object + 1) * sizeof *p->object);
	p->object[p->num_object++] = o;
}

static struct pdf *
pdf_new(void)
{
	struct pdf *p = malloc(sizeof *p);

	memset(p, 0, sizeof *p);

	struct object *catalog = object_new(OBJECT_DICTIONARY);
	dictionary_add(catalog, "Type", object_new_name("Catalog"));
	dictionary_add(catalog, "Pages", object_new_indirect_reference(2, 0));
	struct object *pages = object_new(OBJECT_DICTIONARY);
	dictionary_add(pages, "Type", object_new_name("Pages"));

	struct object *font = object_new(OBJECT_DICTIONARY);
	dictionary_add(font, "Type", object_new_name("Font"));
	dictionary_add(font, "Subtype", object_new_name("Type1"));
	dictionary_add(font, "BaseFont", object_new_name("Times-Roman"));

	pdf_add_object(p, catalog);
	pdf_add_object(p, pages);
	pdf_add_object(p, font);

	return p;
}

static struct page *
pdf_add_page(struct pdf *p)
{
	struct page *page = malloc(sizeof *page);

	page->data = object_new(OBJECT_DICTIONARY);
	dictionary_add(page->data, "Type", object_new_name("Page"));
	dictionary_add(page->data, "Parent", object_new_indirect_reference(2, 0));
	dictionary_add(page->data, "Contents", object_new_indirect_reference(p->num_object + 2, 0));

	struct object *contents = object_new(OBJECT_STREAM);
	page->contents = contents;

	pdf_add_object(p, page->data);
	pdf_add_object(p, contents);

	p->page = realloc(p->page, (p->num_page + 1) * sizeof *p->page);
	return p->page[p->num_page++] = page;
}

struct stream {
	char *buf;
	int len, size;
};

static struct stream *
stream_new(void)
{
	struct stream *s = malloc(sizeof *s);
	memset(s, 0, sizeof *s);
	return s;
}

static void
stream_write(struct stream *s, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(s->buf + s->len, s->size - s->len, fmt, args);

	if (len >= s->size - s->len) {
		s->buf = realloc(s->buf, s->size + len + 1);
		s->size += len + 1;

		va_start(args, fmt);
		vsnprintf(s->buf + s->len, s->size - s->len, fmt, args);
	}

	va_end(args);
	s->len += len;
}

static void
object_write(struct stream *s, struct object *o)
{
	switch (o->type) {
	case OBJECT_NAME:
		stream_write(s, "/%s", o->s);
		break;
	case OBJECT_NUMBER:
		stream_write(s, "%f", o->n);
		break;
	case OBJECT_ARRAY:
		stream_write(s, "[");

		for (unsigned i = 0; i < o->len; i++) {
			if (i) stream_write(s, " ");
			object_write(s, o->dict[i]);
		}

		stream_write(s, "]");
		break;
	case OBJECT_INDIRECT_REFERENCE:
		stream_write(s, "%u %u R", o->on, o->gn);
		break;
	case OBJECT_DICTIONARY:
		stream_write(s, "<<\n");

		for (unsigned i = 0; i < o->len; i++) {
			stream_write(s, "/%s ", o->key[i]);
			object_write(s, o->dict[i]);
			stream_write(s, "\n");
		}

		stream_write(s, ">>\n");
		break;
	case OBJECT_STREAM:
		stream_write(s, "<<\n/Length %u\n>>\nstream\n%s\n\
endstream\n", strlen(o->s), o->s);
		break;
	default:
		fprintf(stderr, "Could not print object %u\n", o->type);
		exit(1);
	}
}

static void
write_object_entry(struct stream *s, struct object *o, int i)
{
	o->addr = s->len;
	stream_write(s, "%u 0 obj\n", i);
	object_write(s, o);
	stream_write(s, "endobj\n\n");
}

static void
array_add(struct object *o, struct object *v)
{
	o->dict = realloc(o->dict, (o->len + 1) * sizeof *o->dict);
	o->dict[o->len++] = v;
}

static struct {
	double width, height;
} page_size[] = {
	{612, 792},         /* letter */
	{612, 1008},        /* legal */
	{841.89, 1190.551}, /* a3 */
	{595.276, 841.89},  /* a4 */
	{419.528, 595.276}, /* a5 */
	{708.661, 1000.63}, /* b4 */
	{498.898, 708.661}, /* b5 */
	{522, 756},         /* executive */
	{288, 432},         /* us4x6 */
	{288, 576},         /* us4x8 */
	{360, 504},         /* us5x7 */
	{297, 684}          /* comm10 */
};

static struct stream *
pdf_render(struct pdf *p)
{
	struct stream *s = stream_new();

	/* Hngh. */
	struct object *mediabox = object_new(OBJECT_ARRAY);
	struct object *kids = object_new(OBJECT_ARRAY);

	/* TODO: Assumes at least one page. */
	/* TODO: Default page size. */
	array_add(mediabox, object_new_number(0));
	array_add(mediabox, object_new_number(0));
	array_add(mediabox, object_new_number(page_size[p->page[0]->size].width));
	array_add(mediabox, object_new_number(page_size[p->page[0]->size].height));

	for (unsigned i = 0; i < p->num_page; i++) {
		struct object *pagebox = object_new(OBJECT_ARRAY);
		array_add(pagebox, object_new_number(0));
		array_add(pagebox, object_new_number(0));
		array_add(pagebox, object_new_number(page_size[p->page[i]->size].width));
		array_add(pagebox, object_new_number(page_size[p->page[i]->size].height));
		dictionary_add(p->page[i]->data, "MediaBox", pagebox);

		array_add(kids, object_new_indirect_reference(4 + i * 2, 0));
	}

	dictionary_add(p->object[1], "MediaBox", mediabox);
	dictionary_add(p->object[1], "Count", object_new_number(p->num_page));
	dictionary_add(p->object[1], "Kids", kids);

	/* Hagh. */

	stream_write(s, "%%PDF-1.7\n\n");

	for (unsigned i = 0; i < p->num_object; i++)
		write_object_entry(s, p->object[i], i + 1);

	stream_write(s, "xref\n0 %u\n", p->num_object);
	stream_write(s, "0000000000 65535 f\n");

	for (unsigned i = 0; i < p->num_object; i++) {
		struct object *o = p->object[i];
		stream_write(s, "%010u 00000 n\n", o->addr);
	}

	unsigned len = s->len;
	stream_write(s, "trailer\n<<\n/Size %u\n/Root 1 0 R\n>>\n", p->num_object);
	stream_write(s, "startxref\n%u\n", len);
	stream_write(s, "%%%%EOF\n");

	return s;
}

static void
pdf_stream(struct page *p, const char *s)
{
	if (!p->contents->s) {
		p->contents->s = malloc(15);
		*p->contents->s = 0;
	}

	p->contents->s = realloc(p->contents->s, strlen(p->contents->s) + strlen(s) + 1);
	strcat(p->contents->s, s);
}

int main(void)
{
	struct pdf *pdf  = pdf_new();

	if (!pdf) {
		fputs("Could not create pdf object.", stderr);
		exit(1);
	}

	pdf->compression = PDF_COMP_ALL;
	pdf->page_mode   = PDF_PAGE_MODE_USE_OUTLINE;

	struct page *p1 = pdf_add_page(pdf);
	struct page *p2 = pdf_add_page(pdf);
	struct page *p3 = pdf_add_page(pdf);
	struct page *p4 = pdf_add_page(pdf);

	p1->size      = PDF_PAGE_SIZE_LETTER;
	p2->size      = PDF_PAGE_SIZE_LETTER;
	p3->size      = PDF_PAGE_SIZE_LETTER;
	p4->size      = PDF_PAGE_SIZE_LETTER;
	p1->direction = PDF_PAGE_LANDSCAPE;
	p2->direction = PDF_PAGE_LANDSCAPE;
	p3->direction = PDF_PAGE_LANDSCAPE;
	p4->direction = PDF_PAGE_LANDSCAPE;

	pdf_stream(p4, "0.9 0.5 0.0 rg 100 400 300 300 re f");

	pdf_stream(p3, "BT\n\
306 200 TD\n\
/F1 12 Tf\n\
(quz quz quz) Tj\n\
ET");

	pdf_stream(p2, "175 720 m 175 500 l 300 800 400 600 v 100 650 50 75 re h S\n");
	pdf_stream(p2, "BT\n\
70 750 TD\n\
/F1 12 Tf\n\
(foo bar baz) Tj\n\
ET");

	pdf_stream(p1, "10 10 m 300 700 l h S\n");
	pdf_stream(p1, "BT\n\
70 750 TD\n\
/F1 12 Tf\n\
(Hello, world!) Tj\n\
ET");

	struct stream *s = pdf_render(pdf);
	for (int i = 0; i < s->len; i++) putchar(s->buf[i]);

	return 0;
}
