INSERT INTO menu (Item, Category, "Price (USD)", "In Stock") 
VALUES ('Flat White', 'Coffee', 4.25, 1);

UPDATE menu 
SET "Price (USD)" = 4.50 
WHERE Item = 'Flat White';

SELECT * FROM menu