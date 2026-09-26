package main

import (
	"log"
	"strconv"

	"github.com/gofiber/fiber/v2"
	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"
)

// Byte-identical to the query in ../../db_pg.c so all three servers do the same DB work.
const articlesSQL = `SELECT json_build_object(
  'articles', COALESCE((
    SELECT json_agg(json_build_object(
      'slug', a.slug,
      'title', a.title,
      'description', a.description,
      'body', a.body,
      'createdAt', to_char(a.created_at, 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'),
      'updatedAt', to_char(a.updated_at, 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'),
      'favorited', EXISTS(SELECT 1 FROM favorites f WHERE f.article_id = a.id AND f.user_id = $1),
      'favoritesCount', (SELECT COUNT(*) FROM favorites f WHERE f.article_id = a.id),
      'author', json_build_object(
        'username', u.username,
        'bio', COALESCE(u.bio, ''),
        'image', COALESCE(u.image, ''),
        'following', EXISTS(SELECT 1 FROM follows fw WHERE fw.follower_id = $1 AND fw.followed_id = u.id)
      ),
      'tagList', COALESCE((
        SELECT json_agg(t.name)
        FROM article_tags at
        JOIN tags t ON t.id = at.tag_id
        WHERE at.article_id = a.id
      ), '[]'::json)
    ))
    FROM (
      SELECT * FROM articles
      ORDER BY created_at DESC
      LIMIT $2 OFFSET $3
    ) a
    JOIN users u ON u.id = a.author_id
  ), '[]'::json),
  'articlesCount', (SELECT article_count FROM article_stats)
)::text;`

func main() {
	db, err := sqlx.Connect("postgres", "host=localhost user=postgres password=postgres dbname=realworld sslmode=disable")
	if err != nil {
		log.Fatalln(err)
	}

	// Match CExpress cluster concurrency (4 workers x 1 connection)
	db.SetMaxOpenConns(4)
	db.SetMaxIdleConns(4)

	app := fiber.New(fiber.Config{
		DisableStartupMessage: true,
	})

	// Static baseline: measures the framework alone, no DB.
	app.Get("/api/ping", func(c *fiber.Ctx) error {
		return c.SendString("pong")
	})

	app.Get("/api/articles", func(c *fiber.Ctx) error {
		limit, _ := strconv.Atoi(c.Query("limit", "20"))
		offset, _ := strconv.Atoi(c.Query("offset", "0"))

		var result string
		err := db.Get(&result, articlesSQL, 0, limit, offset)
		if err != nil {
			return c.Status(500).SendString(err.Error())
		}

		c.Type("json")
		return c.SendString(result)
	})

	log.Println("Go/Fiber listening on :8082")
	log.Fatal(app.Listen(":8082"))
}
