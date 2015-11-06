/* This file is part of ISAAC.
 *
 * ISAAC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * ISAAC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Lesser Public
 * License along with ISAAC.  If not, see <www.gnu.org/licenses/>. */

#include "SDLImageConnector.hpp"

SDLImageConnector::SDLImageConnector()
{
	SDL_Init(SDL_INIT_VIDEO);
	group = NULL;
}

std::string SDLImageConnector::getName()
{
	return "SDLImageConnector";
}

errorCode SDLImageConnector::init(int port)
{
	return 0;
}

errorCode SDLImageConnector::run()
{
	window = SDL_SetVideoMode( 512, 512, 32, SDL_HWSURFACE );
	int finish = 0;
	while (finish == 0)
	{
		SDL_Event event;
		while ( SDL_PollEvent( &event ) == 1 )
		{
			if (event.type == SDL_QUIT)
				finish = 2;
		}
		ImageBufferContainer* message;
		while (message = clientGetMessage())
		{
			if (message->type == IMG_FORCE_EXIT)
				finish = 1;
			else
			if (message->type == GROUP_FINISHED)
			{
				if (group == message->group)
					group = NULL;
				SDL_FreeSurface(window);
				window = SDL_SetVideoMode( 512, 512, 32, SDL_HWSURFACE );
			}
			else
			{
				if (group == NULL)
				{
					group = message->group;
					SDL_FreeSurface(window);
					window = SDL_SetVideoMode( group->getFramebufferWidth(), group->getFramebufferHeight(), 32, SDL_HWSURFACE );
				}
				if (group == message->group) //We show always the very first group
				{
					SDL_LockSurface( window );
					memcpy(window->pixels,message->buffer,message->group->getVideoBufferSize());
					SDL_UnlockSurface( window );
					SDL_Flip(window);
				}
			}
			message->suicide();
		}
		usleep(1000);
	}
	SDL_FreeSurface(window);
	SDL_Quit();
	//Even if the window is close we still need to free the video buffers!
	while (finish == 2)
	{
		ImageBufferContainer* message;
		while (message = clientGetMessage())
		{
			if (message->type == IMG_FORCE_EXIT)
				finish = 1;
			message->suicide();
		}
		usleep(1000);
	}	
}