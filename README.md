# mw3cheat
all in one cod mw3 cheat with insane features

# why?
it was a project me and my friends made and we used to sell it for insane prices ($2000 to russians, $5600 to the rich chinese), but it's died now, and we've decided to release it

# todo
- fix offsets
- redo the ac bypass ( kind of outdated )
- make better menu (?)
- fix keycache

# building
### it's not a build n go project, so learn c++ first

# to start building, open the project and copy it in VS22 or VS19, use C++20
## include direct x sdk imgui and sql
### once included, do the following below

> [!IMPORTANT]  
> you gotta use sql or just remove the antipaste from it

1. set build as dll
2. go into main.cpp and change sql or completely remove everything that requires sql
3. go to client.h and set your needs
4. download mysql binaries and turn mysql on on your vps or local machine
5. make an sql table
6. put it onto your mysql db
7. add a role object onto the table and name it 'beta'
8. make a user on your db and set the role to be beta
9. from client.h change your rpl version (or use 5.5.5 to skip this step)
10. change #define CLIENT_ODBC		64	/* Odbc client */ to what your db pleases
11. delete violite.h if you wanna use local machine hosting
12. build solution

> [!TIP]
> Fix offsets to make the dll not crash the game (you may get detected if you inject without any updated offset)
