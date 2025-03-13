import express from 'express';
import { OpenAI } from 'openai';
import { Buffer } from 'buffer';
import fs from 'fs';
import path from 'path';
import 'dotenv/config';

const app = express();
const port = process.env.PORT || 3000;

// Use raw middleware to parse incoming MP3 binary data
app.use('/process-audio', express.raw({ type: 'audio/wav', limit: '50mb' }));

const openai = new OpenAI(process.env.OPENAI_API_KEY);

app.post('/process-audio', async (req, res) => {
  console.log('Processing audio...');
  try {
    // req.body is a Buffer containing the raw binary data
    const audioBuffer = req.body;
    // Convert the MP3 audio to a base64-encoded string
    const base64Audio = audioBuffer.toString('base64');

    // Send the audio to OpenAI's API
    const response = await openai.chat.completions.create({
      model: 'gpt-4o-audio-preview',
      modalities: ['text', 'audio'],
      audio: { voice: 'alloy', format: 'wav' },
      messages: [
        {
          role: 'user',
          content: [
            { type: 'text', text: 'What is in this recording?' },
            { type: 'input_audio', input_audio: { data: base64Audio, format: 'wav' } }
          ]
        }
      ],
      store: true,
    });

    console.log('OpenAI response:', response);
    console.log('OpenAI response:', response.choices[0].message);
    const audioContent = response.choices[0]?.message?.audio.data;
    if (!audioContent) {
      return res.status(500).send('Audio content missing in API response');
    }

    // Save the base64 string to a text file named 'audio.txt'
    const base64FilePath = path.join('audio.txt');
    fs.writeFileSync(base64FilePath, audioContent, 'utf8');
    console.log('Base64 string saved to', base64FilePath);

    // Decode the base64 audio content
    const audioBufferFromResponse = Buffer.from(audioContent, 'base64');

    // Define the path to save the MP3 file
    const outputFilePath = path.join('output_server.mp3');

    // Write the buffer to a file
    fs.writeFile(outputFilePath, audioBufferFromResponse, (err) => {
      if (err) {
        console.error('Error saving audio file:', err);
        return res.status(500).send('Error saving audio file');
      }
      console.log('Audio file saved to', outputFilePath);
    });

    // Set headers for an MP3 response
    res.setHeader('Content-Type', 'audio/mpeg');

    // Stream the audio buffer in chunks
    const chunkSize = 1024; // 1KB chunks
    for (let i = 0; i < audioBufferFromResponse.length; i += chunkSize) {
      res.write(audioBufferFromResponse.slice(i, i + chunkSize));
    }
    res.end();
  } catch (error) {
    console.error('Error processing audio:', error);
    res.status(500).send('Error processing audio');
  }
});

app.listen(port, () => {
  console.log(`Server running on port ${port}`);
});

export default app;
