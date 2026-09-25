import sqlite3
import string
import random
from datetime import datetime, timedelta
import sys

# Connect to database
conn = sqlite3.connect('realworld.db')
# Enable foreign keys and PRAGMAs for fast insertion
conn.execute('PRAGMA foreign_keys = OFF;')
conn.execute('PRAGMA synchronous = OFF;')
conn.execute('PRAGMA journal_mode = MEMORY;')

cursor = conn.cursor()

def random_string(length=10):
    return ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))

def populate(num_users=100_000, num_articles_per_user=5, num_comments_per_article=2, max_tags=500):
    print(f"Populating DB with {num_users} users, ~{num_users * num_articles_per_user} articles...")
    
    # 1. Insert Tags
    tags = []
    for i in range(max_tags):
        tags.append((f"tag_{i}_{random_string(5)}",))
    cursor.executemany("INSERT OR IGNORE INTO tags (name) VALUES (?)", tags)
    print(f"Inserted tags")
    
    # 2. Insert Users
    users = []
    for i in range(num_users):
        username = f"user_{i}_{random_string(6)}"
        email = f"{username}@example.com"
        password_hash = "hashed_password"
        bio = "This is a bio for " + username
        image = "https://example.com/image.jpg"
        users.append((username, email, password_hash, bio, image))
    
    cursor.executemany("INSERT INTO users (username, email, password_hash, bio, image) VALUES (?, ?, ?, ?, ?)", users)
    print(f"Inserted {num_users} users")
    
    # Get user IDs
    cursor.execute("SELECT id FROM users")
    user_ids = [row[0] for row in cursor.fetchall()]
    
    # 3. Insert Articles
    articles = []
    article_tags = []
    favorites = []
    
    cursor.execute("SELECT id FROM tags")
    tag_ids = [row[0] for row in cursor.fetchall()]
    
    for uid in user_ids:
        # Each user creates some articles
        for j in range(num_articles_per_user):
            title = f"Article {j} by User {uid} {random_string(8)}"
            slug = title.replace(" ", "-").lower()
            desc = "Description for " + title
            body = "Body content for " + title + "\n" * 10
            now_str = datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%S.000Z')
            articles.append((slug, title, desc, body, now_str, now_str, uid))
            
    cursor.executemany("INSERT INTO articles (slug, title, description, body, created_at, updated_at, author_id) VALUES (?, ?, ?, ?, ?, ?, ?)", articles)
    print(f"Inserted {len(articles)} articles")
    
    # Get article IDs
    cursor.execute("SELECT id FROM articles")
    article_ids = [row[0] for row in cursor.fetchall()]
    
    # Populate relationships
    for aid in article_ids:
        # Article Tags
        num_tags = random.randint(1, 5)
        selected_tags = random.sample(tag_ids, min(num_tags, len(tag_ids)))
        for tid in selected_tags:
            article_tags.append((aid, tid))
        
    cursor.executemany("INSERT OR IGNORE INTO article_tags (article_id, tag_id) VALUES (?, ?)", article_tags)
    print("Inserted article_tags")
    
    # Comments
    comments = []
    for aid in article_ids:
        for _ in range(num_comments_per_article):
            uid = random.choice(user_ids)
            body = f"Comment by {uid} on article {aid}"
            now_str = datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%S.000Z')
            comments.append((body, now_str, now_str, uid, aid))
            
    cursor.executemany("INSERT INTO comments (body, created_at, updated_at, author_id, article_id) VALUES (?, ?, ?, ?, ?)", comments)
    print(f"Inserted {len(comments)} comments")
    
    # Favorites
    for aid in article_ids:
        num_favs = random.randint(0, 3)
        fav_users = random.sample(user_ids, min(num_favs, len(user_ids)))
        for uid in fav_users:
            favorites.append((uid, aid))
            
    cursor.executemany("INSERT OR IGNORE INTO favorites (user_id, article_id) VALUES (?, ?)", favorites)
    print(f"Inserted favorites")
    
    # Follows
    follows = []
    for uid in user_ids:
        num_follows = random.randint(0, 5)
        following = random.sample(user_ids, min(num_follows, len(user_ids)))
        for fid in following:
            if uid != fid:
                follows.append((uid, fid))
                
    cursor.executemany("INSERT OR IGNORE INTO follows (follower_id, followed_id) VALUES (?, ?)", follows)
    print("Inserted follows")
    
    conn.commit()
    print("Done!")

if __name__ == "__main__":
    import sys
    scale = 1
    if len(sys.argv) > 1:
        scale = int(sys.argv[1])
    populate(num_users=1000 * scale, num_articles_per_user=5, num_comments_per_article=2)
    conn.close()
