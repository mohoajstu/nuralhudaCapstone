async function sendAudio(audioBlob) {
  const response = await fetch('https://your-firebase-url/process-audio', {
    method: 'POST',
    body: audioBlob,
    headers: {
      'Content-Type': 'audio/wav' // Match your audio format
    }
  });

  const reader = response.body.getReader();
  
  while(true) {
    const { done, value } = await reader.read();
    if(done) break;
    const chunk = new TextDecoder().decode(value);
    console.log('Received chunk:', chunk);
  }
}