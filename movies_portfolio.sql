SELECT DISTINCT people.name FROM
people JOIN
(
    stars JOIN movies
    ON stars.movie_id = movies.id
)
ON people.id = stars.person_id
WHERE people.name != "Kevin Bacon" AND movies.id IN
(
    SELECT movies.id FROM
        movies JOIN
        (
            stars JOIN people
            ON stars.person_id = people.id
        )
        ON movies.id = stars.movie_id
        WHERE people.name = "Kevin Bacon" AND people.birth = "1958"
)
ORDER BY name;
