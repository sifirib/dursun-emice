from pydantic import BaseModel


class ChatResponse(BaseModel):
    success: bool
    text: str