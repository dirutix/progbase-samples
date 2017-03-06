#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <list.h>
#include <events.h>
#include <progbase.h>
#include <pbconsole.h>

typedef struct KeyInputEvent KeyInputEvent;
struct KeyInputEvent {
	char keyCode; 
};
enum { KeyInputEventTypeId = 47578 };

Event * KeyInputEvent_new(void * sender, char key) {
	KeyInputEvent * data = malloc(sizeof(struct KeyInputEvent));
	data->keyCode = key;
	return Event_new(sender, KeyInputEventTypeId, data, NULL);
}

typedef struct RandomNumberEvent RandomNumberEvent;
struct RandomNumberEvent {
	int number;
};
enum { RandomNumberEventTypeId = 134134 };

Event * RandomNumberEvent_new(void * sender, int number) {
	RandomNumberEvent * data = malloc(sizeof(struct RandomNumberEvent));
	data->number = number;
	return Event_new(sender, RandomNumberEventTypeId, data, free);
}

void RandomEventGen_update(void * self, Event * event);
void InputManager_update(void * self, Event * event);
void CustomHandler_handleEvent(void * self, Event * event);
void Timer_handleEvent(void * self, Event * event);

enum { 
	UpdateEventTypeId = 0,
	StartEventTypeId = 1,
	RemoveHandlerEventTypeId = 767456
};

typedef struct HandlerObject HandlerObject;
struct HandlerObject {
	void * self;
	Destructor destructor;
	EventHandler handler;
};

HandlerObject * HandlerObject_new(void * data, Destructor dest, EventHandler handler) {
	HandlerObject * self = malloc(sizeof(HandlerObject));
	self->self = data;
	self->destructor = dest;
	self->handler = handler; 
	return self;
}

void HandlerObject_free(HandlerObject ** selfPtr) {
	HandlerObject * self = *selfPtr;
	if (self->destructor != NULL && self->self != NULL) {
		self->destructor(self->self);
	}
	free(self);
	*selfPtr = NULL;
}

typedef struct EventSystem EventSystem;
struct EventSystem {
	List * handlers;
	EventQueue * events;
};

EventSystem * g_eventSystem = NULL;

void EventSystem_addHandler(HandlerObject * handler);
void EventSystem_removeHandler(void * handler);
void EventSystem_raiseEvent(Event * event);

static void __removeHandlerWithData(void * data);

int main(void) {
	g_eventSystem = &(EventSystem) { 
		.handlers = List_new(), 
		.events = EventQueue_new()
	};

	List_add(g_eventSystem->handlers, HandlerObject_new(NULL, NULL, RandomEventGen_update));
	List_add(g_eventSystem->handlers, HandlerObject_new(NULL, NULL, InputManager_update));
	int counter = 0;
	List_add(g_eventSystem->handlers, HandlerObject_new(&counter, NULL, CustomHandler_handleEvent));
	int timeCounter = 100;
	List_add(g_eventSystem->handlers, HandlerObject_new(&timeCounter, NULL, Timer_handleEvent));

	EventQueue_enqueue(
		g_eventSystem->events, 
		Event_new(NULL, StartEventTypeId, NULL, NULL));
	while (1) {
		EventQueue_enqueue(
			g_eventSystem->events, 
			Event_new(NULL, UpdateEventTypeId, NULL, NULL));
		while (EventQueue_size(g_eventSystem->events) > 0) {
			Event * event = EventQueue_dequeue(g_eventSystem->events);
			for (int i = 0; i < List_count(g_eventSystem->handlers); i++) {
				HandlerObject * handler = List_get(g_eventSystem->handlers, i);
				handler->handler(handler->self, event);
			}
			if (event->type == RemoveHandlerEventTypeId) {
				__removeHandlerWithData(event->data);
			}
			Event_free(&event);
		}
		sleepMillis(33);  // ~ 30 FPS
	}
	EventQueue_free(&g_eventSystem->events);
	for (int i = 0; i < List_count(g_eventSystem->handlers); i++) {
		HandlerObject * handler = List_get(g_eventSystem->handlers, i);
		HandlerObject_free(&handler);
	}
	List_free(&g_eventSystem->handlers);
	return 0;
}

void EventSystem_addHandler(HandlerObject * handler) {
	List_add(g_eventSystem->handlers, handler);
}

void EventSystem_removeHandler(void * handler) {
	EventSystem_raiseEvent(Event_new(NULL, RemoveHandlerEventTypeId, handler, NULL));
}

void EventSystem_raiseEvent(Event * event) {
	EventQueue_enqueue(g_eventSystem->events, event);
}

static void __removeHandlerWithData(void * data) {
	HandlerObject * handlerToRemove = NULL;	
	for (int i = 0; i < List_count(g_eventSystem->handlers); i++) {
		HandlerObject * handler = List_get(g_eventSystem->handlers, i);
		if (handler->self == data) {
			handlerToRemove = handler;
			break;
		}
	}
	if (handlerToRemove != NULL) {
		List_remove(g_eventSystem->handlers, handlerToRemove);
		HandlerObject_free(&handlerToRemove);
	}
}

void RandomEventGen_update(void * self, Event * event) {
	if (event->type == StartEventTypeId) {
		srand(time(0));
	}
	if (rand() % 33 == 0) {
		int number = rand() % 200 - 100;
		EventQueue_enqueue(
			g_eventSystem->events, 
			RandomNumberEvent_new(self, number));
	}
}

void InputManager_update(void * self, Event * event) {
	if (conIsKeyDown()) {
		char keyCode = getchar();
		EventQueue_enqueue(
			g_eventSystem->events, 
			KeyInputEvent_new(self, keyCode));
	}
}

enum { CustomEventTypeId = 124090 };

void CustomHandler_handleEvent(void * self, Event * event) {
	switch (event->type) {
		case StartEventTypeId: {
			puts("START!");
			break;
		}
		case UpdateEventTypeId: {
			putchar('.');
			break;
		}
		case RandomNumberEventTypeId: {
			RandomNumberEvent * randEvent = (RandomNumberEvent *)event->data;
			printf("Random number %i\n", randEvent->number);
			break;
		}
		case KeyInputEventTypeId: {
			KeyInputEvent * keyEvent = (KeyInputEvent *)event->data;
			if (keyEvent->keyCode == ' ') {
				EventQueue_enqueue(
					g_eventSystem->events, 
					Event_new(self, CustomEventTypeId, NULL, NULL));
			}
			if (keyEvent->keyCode == 'a') {
				int * timer = malloc(sizeof(int));
				*timer = 50;
				HandlerObject * handler = malloc(sizeof(HandlerObject));
				handler->self = timer;
				handler->handler = Timer_handleEvent;
				handler->destructor = free;
				EventSystem_addHandler(handler);
			}
			printf("Key pressed `%c` [%i]\n", 
				keyEvent->keyCode, keyEvent->keyCode);
			break;
		}
		case CustomEventTypeId: {
			int * counterPtr = (int *)self;
			(*counterPtr)++;
			printf(">>> Custom event! Counter: %i\n", *counterPtr);
			break;
		}
	} 
}

void Timer_handleEvent(void * self, Event * event) {
	switch(event->type) {
		case UpdateEventTypeId: {
			int * timePtr = (int *)self;
			*timePtr -= 1;
			if (*timePtr % 10 == 0) {
				printf("\nTimer: %i\n", *timePtr); 
			}
			if (*timePtr == 0) {
				printf("\nTimer COMPLETED!\n"); 
				EventQueue_enqueue(
					g_eventSystem->events, 
					Event_new(self, RemoveHandlerEventTypeId, self, NULL));
			}
			break;
		}
	}
}