package main

import (
	"log"
	"strconv"

	"github.com/gofiber/fiber/v2"
	"github.com/jmoiron/sqlx"
	_ "github.com/mattn/go-sqlite3"
)

func main() {
	db, err := sqlx.Connect("sqlite3", "../../realworld.db")
	if err != nil {
		log.Fatalln(err)
	}

	// Match CExpress cluster concurrency
	db.SetMaxOpenConns(4)
	db.SetMaxIdleConns(4)

	app := fiber.New(fiber.Config{
		DisableStartupMessage: true,
	})

	app.Get("/api/articles", func(c *fiber.Ctx) error {
		limit, _ := strconv.Atoi(c.Query("limit", "20"))
		offset, _ := strconv.Atoi(c.Query("offset", "0"))

		sqlQuery := `
		SELECT json_object(
		  'articles', COALESCE((
		    SELECT json_group_array(
		      json_object(
		        'slug', a.slug,
		        'title', a.title,
		        'description', a.description,
		        'body', a.body,
		        'createdAt', a.created_at,
		        'updatedAt', a.updated_at,
		        'favorited', EXISTS(SELECT 1 FROM favorites f WHERE f.article_id = a.id AND f.user_id = ?1),
		        'favoritesCount', (SELECT COUNT(*) FROM favorites f WHERE f.article_id = a.id),
		        'author', json_object(
		          'username', u.username,
		          'bio', u.bio,
		          'image', u.image,
		          'following', EXISTS(SELECT 1 FROM follows fw WHERE fw.follower_id = ?1 AND fw.followed_id = u.id)
		        ),
		        'tagList', COALESCE((
		          SELECT json_group_array(t.name)
		          FROM article_tags at
		          JOIN tags t ON t.id = at.tag_id
		          WHERE at.article_id = a.id
		        ), json_array())
		      )
		    )
		    FROM (
		      SELECT * FROM articles
		      ORDER BY created_at DESC
		      LIMIT ?2 OFFSET ?3
		    ) a
		    JOIN users u ON u.id = a.author_id
		  ), json_array()),
		  'articlesCount', (SELECT COUNT(*) FROM articles)
		);`

		var result string
		err := db.Get(&result, sqlQuery, 0, limit, offset)
		if err != nil {
			return c.Status(500).SendString(err.Error())
		}

		c.Type("json")
		return c.SendString(result)
	})

	log.Println("Go/Fiber listening on :8082")
	log.Fatal(app.Listen(":8082"))
}
