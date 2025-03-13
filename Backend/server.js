const functions = require('firebase-functions');
const express = require('express');
const cors = require('cors');
const OpenAI = require('openai');

const openai = new OpenAI({
  apiKey: process.env.OPENAI_API_KEY,
});

const app = express();

// Middleware to handle raw audio data and CORS
app.use(cors({ origin: true }));
app.use(express.raw({
  type: ['audio/mp3'],
  limit: '10mb' // Adjust based on your needs
}));

app.post('/process-audio', async (req, res) => {
  try {
    // Convert received audio buffer to base64
    const base64Audio = req.body.toString('base64');

    // Create OpenAI streaming request
    const stream = await openai.chat.completions.create({
      model: "gpt-4o-audio-preview",
      modalities: ["text", "audio"],
      audio: { voice: "alloy", format: "mp3" },
      messages: [{
        role: "user",
        content: [
          { type: "text", text: "What is in this recording?" },
          { type: "input_audio", input_audio: { 
            data: base64Audio, 
            format: "mp3" 
          }}
        ]
      }],
      stream: true,
    });

    // Set headers for chunked transfer
    res.setHeader('Content-Type', 'text/plain');
    res.setHeader('Transfer-Encoding', 'chunked');

    // Stream OpenAI responses to client
    for await (const chunk of stream) {
      const content = chunk.choices[0]?.delta?.content || '';
      res.write(content);
    }

    res.end();
  } catch (error) {
    console.error('Error processing audio:', error);
    res.status(500).send(error.message);
  }
});

exports.audioProcessing = functions.https.onRequest(app);